#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/types.h>

#define SHM_SIGNATURE UINT32_C(0x4D335434) //M3T4 in ASCII
#define SHM_VERSION UINT32_C(2)
#define SHM_NULL_OFFSET UINT64_C(0)

typedef uint64_t shm_offset_t;
typedef int32_t shm_value_t;

typedef enum {
    SHARED_BLOCK_READY,
    SHARED_BLOCK_IN_PROGRESS,
    SHARED_BLOCK_PROCESSED
} shared_block_state_t;

typedef struct {
    uint32_t signature;
    uint32_t version;

    uint64_t segment_size;
    shm_offset_t first_offset;
    shm_offset_t free_offset;

    uint64_t published_blocks;
    uint64_t processed_blocks;

    uint32_t producer_done;
    uint32_t active_consumers;
} shared_header_t;

typedef struct {
    shm_offset_t next_offset;
    uint32_t count;
    uint32_t original_count;
    uint32_t state;
    shm_value_t values[];
} shared_block_t;

bool ipc_make_keys(key_t *shm_key, key_t *sem_key);

bool shared_block_size(uint32_t count, size_t *result);
bool shared_block_is_valid(const void *base, const shared_header_t *header, shm_offset_t offset);

bool shared_header_initialize(void *base, size_t segment_size);
bool shared_header_is_valid(const shared_header_t *header, size_t actual_segment_size);

bool ipc_semaphore_create(key_t key, int *sem_id, bool *created);
bool ipc_semaphore_open(key_t key, int *sem_id);
bool ipc_semaphore_lock(int sem_id);
bool ipc_semaphore_unlock(int sem_id);
bool ipc_semaphore_remove(int sem_id);

bool ipc_shared_memory_create(key_t key, size_t size, int *shm_id, bool *created);
bool ipc_shared_memory_open(key_t key, int *shm_id, size_t *actual_size);
bool ipc_shared_memory_attach(int shm_id, void **base);
bool ipc_shared_memory_detach(void *base);
bool ipc_shared_memory_remove(int shm_id);

shared_block_t *shared_block_at(
    void *base, const shared_header_t *header, shm_offset_t offset
);
const shared_block_t *shared_block_at_const(
    const void *base, const shared_header_t *header, shm_offset_t offset
);
