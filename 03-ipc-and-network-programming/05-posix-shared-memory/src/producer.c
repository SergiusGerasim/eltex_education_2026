#include "producer.h"

#include "ipc.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef enum {
    PRODUCER_APPEND_OK,
    PRODUCER_APPEND_NO_SPACE,
    PRODUCER_APPEND_ERROR
} producer_append_result_t;

static bool producer_config_is_valid(const producer_config_t *config) {
    if (config == NULL) return false;
    if (config->segment_size < sizeof(shared_header_t)) return false;
    if (config->min_array_length == 0) return false;
    if (config->min_array_length > config->max_array_length) return false;
    if (config->min_value > config->max_value) return false;
    if (config->generation_interval_seconds == 0) return false;
    if (config->check_interval_seconds == 0) return false;

    return true;
}

static uint64_t producer_random_uint64(void) {
    uint64_t value = 0;

    for (unsigned int shift = 0; shift < 64; shift += 16) {
        value ^= (uint64_t)((unsigned int)rand() & UINT16_MAX) << shift;
    }

    return value;
}

static bool producer_generate_values(const producer_config_t *config, uint32_t max_count, shm_value_t **values, uint32_t *count) {
    if (!producer_config_is_valid(config) || values == NULL || count == NULL) return false;
    if (max_count < config->min_array_length || max_count > config->max_array_length) return false;

    const uint64_t count_range = (uint64_t)max_count - config->min_array_length + 1;
    const uint32_t generated_count = config->min_array_length + (uint32_t)(producer_random_uint64() % count_range);

    if (sizeof(shm_value_t) > SIZE_MAX / (size_t)generated_count) return false;

    shm_value_t *generated_values = malloc((size_t)generated_count * sizeof(*generated_values));
    if (generated_values == NULL) return false;

    const uint64_t value_range = (uint64_t)((int64_t)config->max_value - config->min_value) + 1;

    for (uint32_t index = 0; index < generated_count; ++index) {
        const uint64_t offset = producer_random_uint64() % value_range;
        generated_values[index] = (shm_value_t)((int64_t)config->min_value + (int64_t)offset);
    }

    *values = generated_values;
    *count = generated_count;
    return true;
}

static bool producer_max_fitting_count(const shared_header_t *header, uint32_t configured_max, uint32_t *result) {
    if (header == NULL || result == NULL) return false;
    if (header->free_offset > header->segment_size) return false;

    const uint64_t remaining = header->segment_size - header->free_offset;
    uint32_t low = 0;
    uint32_t high = configured_max;

    while (low < high) {
        const uint32_t candidate = low + (uint32_t)(((uint64_t)high - low + 1) / 2);
        size_t block_size;

        if (!shared_block_size(candidate, &block_size)) return false;

        if (block_size <= remaining) low = candidate;
        else high = candidate - 1;
    }

    *result = low;
    return true;
}

static shared_block_t *producer_find_last_block(void *base, shared_header_t *header) {
    if (base == NULL || header == NULL) return NULL;
    if (header->first_offset == SHM_NULL_OFFSET) return NULL;

    shm_offset_t offset = header->first_offset;

    for (uint64_t visited = 0; visited < header->published_blocks; ++visited) {
        shared_block_t *block = shared_block_at(base, header, offset);
        if (block == NULL) return NULL;
        if (block->next_offset == SHM_NULL_OFFSET) return block;

        offset = block->next_offset;
    }

    return NULL;
}

static producer_append_result_t producer_append_block(void *base, shared_header_t *header, const shm_value_t *values, uint32_t count) {
    if (base == NULL || header == NULL || values == NULL) return PRODUCER_APPEND_ERROR;
    if (count == 0) return PRODUCER_APPEND_ERROR;
    if (!shared_header_is_valid(header, (size_t)header->segment_size)) return PRODUCER_APPEND_ERROR;
    if (header->producer_done != 0) return PRODUCER_APPEND_ERROR;
    if (header->published_blocks == UINT64_MAX) return PRODUCER_APPEND_ERROR;

    size_t block_size;
    if (!shared_block_size(count, &block_size)) return PRODUCER_APPEND_ERROR;
    if (header->free_offset > header->segment_size) return PRODUCER_APPEND_ERROR;
    if (block_size > header->segment_size - header->free_offset) return PRODUCER_APPEND_NO_SPACE;

    shared_block_t *last = NULL;
    if (header->first_offset != SHM_NULL_OFFSET) {
        last = producer_find_last_block(base, header);
        if (last == NULL) return PRODUCER_APPEND_ERROR;
    }

    const shm_offset_t new_offset = header->free_offset;
    unsigned char *bytes = base;
    shared_block_t *new_block = (shared_block_t *)(bytes + (size_t)new_offset);

    new_block->next_offset = SHM_NULL_OFFSET;
    new_block->count = count;
    new_block->original_count = count;
    new_block->state = SHARED_BLOCK_READY;
    memcpy(new_block->values, values, (size_t)count * sizeof(*values));

    header->free_offset += block_size;

    if (last == NULL) header->first_offset = new_offset;
    else last->next_offset = new_offset;

    header->published_blocks++;
    return PRODUCER_APPEND_OK;
}

int producer_run(const producer_config_t *config) {
    if (!producer_config_is_valid(config)) return 1;

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    sem_t *semaphore = SEM_FAILED;
    int shm_fd = -1;
    bool sem_created = false;
    bool shm_created = false;
    bool locked = false;
    void *base = NULL;
    int result = 1;

    if (!ipc_semaphore_create(&semaphore)) goto cleanup;
    sem_created = true;
    if (!ipc_shared_memory_create(config->segment_size, &shm_fd)) goto cleanup;
    shm_created = true;
    if (!ipc_shared_memory_attach(shm_fd, config->segment_size, &base)) goto cleanup;
    if (!ipc_semaphore_lock(semaphore)) goto cleanup;
    locked = true;

    shared_header_t *header = base;
    if (!shared_header_initialize(base, config->segment_size)) goto cleanup;

    if (!ipc_semaphore_unlock(semaphore)) goto cleanup;
    locked = false;

    for (;;) {
        shm_value_t *values = NULL;
        uint32_t count = 0;
        uint32_t max_fitting_count = 0;

        if (!ipc_semaphore_lock(semaphore)) goto cleanup;
        locked = true;

        if (!shared_header_is_valid(header, config->segment_size)) goto cleanup;
        if (!producer_max_fitting_count(header, config->max_array_length, &max_fitting_count)) goto cleanup;

        if (max_fitting_count < config->min_array_length) {
            header->producer_done = 1;
            if (!ipc_semaphore_unlock(semaphore)) goto cleanup;
            locked = false;
            break;
        }

        if (!ipc_semaphore_unlock(semaphore)) goto cleanup;
        locked = false;

        if (!producer_generate_values(config, max_fitting_count, &values, &count)) goto cleanup;

        if (!ipc_semaphore_lock(semaphore)) {
            free(values);
            goto cleanup;
        }
        locked = true;

        const producer_append_result_t append_result = producer_append_block(base, header, values, count);
        free(values);

        if (!ipc_semaphore_unlock(semaphore)) goto cleanup;
        locked = false;

        if (append_result == PRODUCER_APPEND_ERROR) goto cleanup;
        if (append_result == PRODUCER_APPEND_NO_SPACE) goto cleanup;
        sleep(config->generation_interval_seconds);
    }

    for (;;) {
        if (!ipc_semaphore_lock(semaphore)) goto cleanup;
        locked = true;

        if (!shared_header_is_valid(header, config->segment_size)) goto cleanup;
        const bool processing_finished = header->processed_blocks == header->published_blocks && header->active_consumers == 0;

        if (!ipc_semaphore_unlock(semaphore)) goto cleanup;
        locked = false;

        if (processing_finished) break;
        sleep(config->check_interval_seconds);
    }

    result = 0;

cleanup:
    if (locked) ipc_semaphore_unlock(semaphore);
    if (base != NULL) ipc_shared_memory_detach(base, config->segment_size);
    if (shm_fd >= 0) ipc_shared_memory_close(shm_fd);
    if (semaphore != SEM_FAILED) ipc_semaphore_close(semaphore);
    if (shm_created) ipc_shared_memory_remove();
    if (sem_created) ipc_semaphore_remove();

    return result;
}
