#include "ipc.h"

#include <stdalign.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/shm.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* Записывает в result, сколько байтов занимает блок с count числами. */
bool shared_block_size(uint32_t count, size_t *result) {
    if (result == NULL) return false;

    const size_t base_size = offsetof(shared_block_t, values);
    const size_t value_size = sizeof(shm_value_t);
    const size_t alignment = alignof(shared_block_t);

    if ((size_t)count > (SIZE_MAX - base_size) / value_size) return false;

    size_t size = base_size + (size_t)count * value_size;
    const size_t remainder = size % alignment;

    if (remainder != 0) {
        const size_t padding = alignment - remainder;
        if (size > SIZE_MAX - padding) return false;
        size += padding;
    }

    *result = size;

    return true;
}

bool shared_header_is_valid(const shared_header_t *header, size_t actual_segment_size) {
    if (header == NULL) return false;
    if (header->signature != SHM_SIGNATURE) return false;
    if (header->version != SHM_VERSION) return false;
    if (header->segment_size != (uint64_t)actual_segment_size) return false;
    if (header->segment_size < sizeof(shared_header_t)) return false;
    if (header->free_offset < sizeof(shared_header_t)) return false;
    if (header->free_offset > header->segment_size) return false;
    if (header->free_offset % alignof(shared_block_t) != 0) return false;
    if (header->producer_done > 1) return false;
    if (header->processed_blocks > header->published_blocks) return false;

    if (header->first_offset == SHM_NULL_OFFSET) {
        if (header->published_blocks != 0) return false;
        if (header->processed_blocks != 0) return false;
        return true;
    }

    if (header->published_blocks == 0) return false;
    if (header->first_offset < sizeof(shared_header_t)) return false;
    if (header->first_offset % alignof(shared_block_t) != 0) return false;
    if (header->first_offset > header->free_offset) return false;
    if (sizeof(shared_block_t) > header->free_offset - header->first_offset) return false;

    return true;
}

bool shared_block_is_valid(const void *base, const shared_header_t *header, shm_offset_t offset) {
    if (base == NULL || header == NULL) return false;
    if (offset == SHM_NULL_OFFSET) return false;
    if (offset < sizeof(shared_header_t)) return false;
    if (offset % alignof(shared_block_t) != 0) return false;
    if (offset > header->free_offset) return false;
    if (sizeof(shared_block_t) > header->free_offset - offset) return false;

    const unsigned char *bytes = base;
    const shared_block_t *block = (const shared_block_t *)(bytes + (size_t)offset);

    if (block->original_count == 0) return false;
    if (block->state > SHARED_BLOCK_PROCESSED) return false;
    if (block->state == SHARED_BLOCK_PROCESSED && block->count != 0) return false;
    if (block->state != SHARED_BLOCK_PROCESSED && block->count != block->original_count) return false;

    size_t block_size;
    if (!shared_block_size(block->original_count, &block_size)) return false;
    if (block_size > header->free_offset - offset) return false;

    if (block->next_offset == SHM_NULL_OFFSET) return true;
    if (block->next_offset <= offset) return false;
    if (block->next_offset % alignof(shared_block_t) != 0) return false;
    if (block->next_offset > header->free_offset) return false;
    if (sizeof(shared_block_t) > header->free_offset - block->next_offset) return false;
    if (block_size > block->next_offset - offset) return false;

    return true;
}

bool ipc_semaphore_lock(int sem_id){
    struct sembuf operation = {.sem_num = 0, .sem_op = -1, .sem_flg = SEM_UNDO};

    while (semop(sem_id, &operation, 1) == -1){
        if (errno != EINTR) return false;
    }
    return true;
}

bool ipc_semaphore_unlock(int sem_id){
    struct sembuf operation = {.sem_num = 0, .sem_op = 1, .sem_flg = SEM_UNDO};

    while (semop(sem_id, &operation, 1) == -1) {
        if (errno != EINTR) return false;
    }

    return true;
}

bool ipc_semaphore_open(key_t key, int *sem_id){
    if (sem_id == NULL) return false;

    int id = semget(key, 1, 0660);
    if (id == -1) return false;

    *sem_id = id;
    return true;
}

bool ipc_semaphore_create(key_t key, int *sem_id, bool *created){
    if (sem_id == NULL || created == NULL) return false;
    int id = semget(key, 1, IPC_CREAT | IPC_EXCL | 0660);
    if (id == -1) {
        if (errno != EEXIST) return false;
        if (!ipc_semaphore_open(key, &id)) return false;

        *sem_id = id;
        *created = false;
        return true;
    }

    union semun argument = {.val = 1};
    if (semctl(id, 0, SETVAL, argument) == -1){
        semctl(id, 0, IPC_RMID);
        return false;
    }

    *sem_id = id;
    *created = true;
    return true;
}

bool ipc_semaphore_remove(int sem_id){
    if (sem_id < 0) return false;

    return semctl(sem_id, 0, IPC_RMID) != -1;
}

bool ipc_shared_memory_attach(int shm_id, void **base) {
    if (shm_id < 0 || base == NULL) return false;

    void *address = shmat(shm_id, NULL, 0);
    if (address == (void *)-1) return false;

    *base = address;
    return true;
}

bool ipc_shared_memory_detach(void *base) {
    if (base == NULL || base == (void *)-1) return false;
    return shmdt(base) != -1;
}

bool ipc_shared_memory_remove(int shm_id) {
    if (shm_id < 0) return false;
    return shmctl(shm_id, IPC_RMID, NULL) != -1;
}

bool ipc_shared_memory_open(key_t key, int *shm_id, size_t *actual_size){
    if (shm_id == NULL || actual_size == NULL) return false;

    int id = shmget(key, 1, 0660);
    if (id == -1) return false;

    struct shmid_ds info;

    if (shmctl(id, IPC_STAT, &info) == -1) return false;

    *shm_id = id;
    *actual_size = (size_t)info.shm_segsz;
    return true;
}

bool ipc_shared_memory_create(key_t key, size_t size, int *shm_id, bool *created){
    if (shm_id == NULL || created == NULL) return false;
    if (size < sizeof(shared_header_t)) return false;

    int id = shmget(key, size, IPC_CREAT | IPC_EXCL | 0660);
    if (id == -1){
        if (errno != EEXIST) return false;
        size_t actual_size;

        if (!ipc_shared_memory_open(key, &id, &actual_size)) return false;
        if (actual_size != size) return false;

        *shm_id = id;
        *created = false;
        return true;
    }

    *shm_id = id;
    *created = true;
    return true;
}

bool shared_header_initialize(void *base, size_t segment_size){
    if (base == NULL) return false;
    size_t free_offset = sizeof(shared_header_t);
    size_t alignment = alignof(shared_block_t);
    size_t remainder = free_offset % alignment;

    if (remainder != 0) {
        size_t padding = alignment - remainder;

        if (free_offset > SIZE_MAX - padding) return false;
        free_offset += padding;
    }

    if (segment_size < free_offset) return false;

    shared_header_t *header = base;

    header->signature = SHM_SIGNATURE;
    header->version = SHM_VERSION;
    header->segment_size = segment_size;

    header->first_offset = SHM_NULL_OFFSET;
    header->free_offset = free_offset;

    header->published_blocks = 0;
    header->processed_blocks = 0;

    header->producer_done = 0;
    header->active_consumers = 0;

    return shared_header_is_valid(header, segment_size);
}

bool ipc_make_keys(key_t *shm_key, key_t *sem_key) {
    if (shm_key == NULL || sem_key == NULL) return false;

    key_t memory_key = ftok("/proc/self/exe", 'M');
    if (memory_key == (key_t)-1) return false;

    key_t semaphore_key = ftok("/proc/self/exe", 'S');
    if (semaphore_key == (key_t)-1) return false;

    if (memory_key == semaphore_key) return false;

    *shm_key = memory_key;
    *sem_key = semaphore_key;
    return true;
}

shared_block_t *shared_block_at(void *base, const shared_header_t *header, shm_offset_t offset) {
    if (!shared_block_is_valid(base, header, offset)) return NULL;

    unsigned char *bytes = base;
    return (shared_block_t *)(bytes + (size_t)offset);
}

const shared_block_t *shared_block_at_const(const void *base, const shared_header_t *header, shm_offset_t offset) {
    if (!shared_block_is_valid(base, header, offset)) return NULL;

    const unsigned char *bytes = base;
    return (const shared_block_t *)(bytes + (size_t)offset);
}
