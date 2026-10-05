#pragma once

#include <stdbool.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>

#define IPC_SHM_NAME "/module3_task5_shm"
#define IPC_SEM_NAME "/module3_task5_sem"
#define SHM_SIGNATURE UINT32_C(0x4D335435) // M3T5 in ASCII
#define SHM_VERSION UINT32_C(3)
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

bool shared_block_size(uint32_t count, size_t *result);
bool shared_block_is_valid(const void *base, const shared_header_t *header, shm_offset_t offset);

bool shared_header_initialize(void *base, size_t segment_size);
bool shared_header_is_valid(const shared_header_t *header, size_t actual_segment_size);

bool ipc_semaphore_create(sem_t **semaphore);
bool ipc_semaphore_open(sem_t **semaphore);
bool ipc_semaphore_lock(sem_t *semaphore);
bool ipc_semaphore_unlock(sem_t *semaphore);
bool ipc_semaphore_close(sem_t *semaphore);
bool ipc_semaphore_remove(void);

bool ipc_shared_memory_create(size_t size, int *fd);
bool ipc_shared_memory_open(int *fd, size_t *actual_size);
bool ipc_shared_memory_attach(int fd, size_t size, void **base);
bool ipc_shared_memory_detach(void *base, size_t size);
bool ipc_shared_memory_close(int fd);
bool ipc_shared_memory_remove(void);

shared_block_t *shared_block_at(
    void *base, const shared_header_t *header, shm_offset_t offset
);
const shared_block_t *shared_block_at_const(
    const void *base, const shared_header_t *header, shm_offset_t offset
);
