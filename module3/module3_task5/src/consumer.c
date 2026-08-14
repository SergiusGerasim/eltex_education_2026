#include "consumer.h"

#include "ipc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
    CONSUMER_TAKE_OK,
    CONSUMER_TAKE_NO_DATA,
    CONSUMER_TAKE_FINISHED,
    CONSUMER_TAKE_ERROR
} consumer_take_result_t;

static bool consumer_config_is_valid(const consumer_config_t *config) {
    if (config == NULL) return false;
    if (config->read_interval_seconds == 0) return false;

    return true;
}

static consumer_take_result_t consumer_take_block(void *base, shared_header_t *header, size_t segment_size, shm_value_t **values, uint32_t *count,
                                                   shm_offset_t *taken_offset) {
    if (base == NULL || header == NULL || values == NULL || count == NULL || taken_offset == NULL) return CONSUMER_TAKE_ERROR;
    if (!shared_header_is_valid(header, segment_size)) return CONSUMER_TAKE_ERROR;

    shm_offset_t offset = header->first_offset;

    for (uint64_t visited = 0; visited < header->published_blocks && offset != SHM_NULL_OFFSET; ++visited) {
        shared_block_t *block = shared_block_at(base, header, offset);
        if (block == NULL) return CONSUMER_TAKE_ERROR;

        if (block->state == SHARED_BLOCK_READY) {
            const uint32_t original_count = block->original_count;
            if (sizeof(shm_value_t) > SIZE_MAX / (size_t)original_count) return CONSUMER_TAKE_ERROR;

            shm_value_t *local_values = malloc((size_t)original_count * sizeof(*local_values));
            if (local_values == NULL) return CONSUMER_TAKE_ERROR;

            memcpy(local_values, block->values, (size_t)original_count * sizeof(*local_values));

            block->state = SHARED_BLOCK_IN_PROGRESS;
            *values = local_values;
            *count = original_count;
            *taken_offset = offset;
            return CONSUMER_TAKE_OK;
        }

        offset = block->next_offset;
    }

    if (offset != SHM_NULL_OFFSET) return CONSUMER_TAKE_ERROR;
    if (header->producer_done != 0 && header->processed_blocks == header->published_blocks) return CONSUMER_TAKE_FINISHED;

    return CONSUMER_TAKE_NO_DATA;
}

static bool consumer_finish_block(void *base, shared_header_t *header, size_t segment_size, shm_offset_t offset) {
    if (!shared_header_is_valid(header, segment_size)) return false;

    shared_block_t *block = shared_block_at(base, header, offset);
    if (block == NULL) return false;
    if (block->state != SHARED_BLOCK_IN_PROGRESS) return false;
    if (block->count != block->original_count) return false;
    if (header->processed_blocks == UINT64_MAX) return false;

    block->count = 0;
    block->state = SHARED_BLOCK_PROCESSED;
    header->processed_blocks++;
    return true;
}

static void consumer_find_min_max(const shm_value_t *values, uint32_t count, shm_value_t *minimum, shm_value_t *maximum) {
    shm_value_t min_value = values[0];
    shm_value_t max_value = values[0];

    for (uint32_t index = 1; index < count; ++index) {
        if (values[index] < min_value) min_value = values[index];
        if (values[index] > max_value) max_value = values[index];
    }

    *minimum = min_value;
    *maximum = max_value;
}

int consumer_run(const consumer_config_t *config) {
    if (!consumer_config_is_valid(config)) return 1;

    sem_t *semaphore = SEM_FAILED;
    int shm_fd = -1;
    size_t segment_size = 0;
    void *base = NULL;
    bool locked = false;
    bool registered = false;
    int result = 1;

    if (!ipc_semaphore_open(&semaphore)) goto cleanup;
    if (!ipc_shared_memory_open(&shm_fd, &segment_size)) goto cleanup;
    if (!ipc_shared_memory_attach(shm_fd, segment_size, &base)) goto cleanup;

    shared_header_t *header = base;

    if (!ipc_semaphore_lock(semaphore)) goto cleanup;
    locked = true;

    if (!shared_header_is_valid(header, segment_size)) goto cleanup;
    if (header->active_consumers == UINT32_MAX) goto cleanup;
    header->active_consumers++;
    registered = true;

    if (!ipc_semaphore_unlock(semaphore)) goto cleanup;
    locked = false;

    for (;;) {
        shm_value_t *values = NULL;
        uint32_t count = 0;
        shm_offset_t taken_offset = SHM_NULL_OFFSET;

        if (!ipc_semaphore_lock(semaphore)) goto cleanup;
        locked = true;

        const consumer_take_result_t take_result = consumer_take_block(base, header, segment_size, &values, &count, &taken_offset);

        if (take_result == CONSUMER_TAKE_FINISHED || take_result == CONSUMER_TAKE_ERROR) {
            if (header->active_consumers == 0) {
                free(values);
                goto cleanup;
            }
            header->active_consumers--;
            registered = false;
        }

        if (!ipc_semaphore_unlock(semaphore)) {
            free(values);
            goto cleanup;
        }
        locked = false;

        if (take_result == CONSUMER_TAKE_ERROR) {
            free(values);
            goto cleanup;
        }

        if (take_result == CONSUMER_TAKE_FINISHED) {
            result = 0;
            break;
        }

        if (take_result == CONSUMER_TAKE_OK) {
            shm_value_t minimum;
            shm_value_t maximum;

            consumer_find_min_max(values, count, &minimum, &maximum);
            printf("consumer %ld: count=%u, min=%d, max=%d\n", (long)getpid(), count, minimum, maximum);
            free(values);

            if (!ipc_semaphore_lock(semaphore)) goto cleanup;
            locked = true;

            if (!consumer_finish_block(base, header, segment_size, taken_offset)) goto cleanup;

            if (!ipc_semaphore_unlock(semaphore)) goto cleanup;
            locked = false;
        }

        sleep(config->read_interval_seconds);
    }

cleanup:
    if (registered && !locked && ipc_semaphore_lock(semaphore)) locked = true;
    if (registered && locked && header->active_consumers != 0) {
        header->active_consumers--;
        registered = false;
    }
    if (locked) ipc_semaphore_unlock(semaphore);
    if (base != NULL) ipc_shared_memory_detach(base, segment_size);
    if (shm_fd >= 0) ipc_shared_memory_close(shm_fd);
    if (semaphore != SEM_FAILED) ipc_semaphore_close(semaphore);
    return result;
}
