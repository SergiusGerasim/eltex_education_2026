#include "ipc.h"

#include <stdalign.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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

bool ipc_semaphore_lock(sem_t *semaphore) {
    if (semaphore == NULL || semaphore == SEM_FAILED) return false;
    while (sem_wait(semaphore) == -1) {
        if (errno != EINTR) return false;
    }
    return true;
}

bool ipc_semaphore_unlock(sem_t *semaphore) {
    if (semaphore == NULL || semaphore == SEM_FAILED) return false;
    return sem_post(semaphore) != -1;
}

bool ipc_semaphore_open(sem_t **semaphore) {
    if (semaphore == NULL) return false;
    sem_t *opened = sem_open(IPC_SEM_NAME, 0);
    if (opened == SEM_FAILED) return false;
    *semaphore = opened;
    return true;
}

bool ipc_semaphore_create(sem_t **semaphore) {
    if (semaphore == NULL) return false;
    sem_t *created = sem_open(IPC_SEM_NAME, O_CREAT | O_EXCL, 0660, 1);
    if (created == SEM_FAILED) return false;
    *semaphore = created;
    return true;
}

bool ipc_semaphore_close(sem_t *semaphore) {
    if (semaphore == NULL || semaphore == SEM_FAILED) return false;
    return sem_close(semaphore) != -1;
}

bool ipc_semaphore_remove(void) {
    return sem_unlink(IPC_SEM_NAME) != -1;
}

bool ipc_shared_memory_attach(int fd, size_t size, void **base) {
    if (fd < 0 || size == 0 || base == NULL) return false;
    void *address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (address == MAP_FAILED) return false;

    *base = address;
    return true;
}

bool ipc_shared_memory_detach(void *base, size_t size) {
    if (base == NULL || base == MAP_FAILED || size == 0) return false;
    return munmap(base, size) != -1;
}

bool ipc_shared_memory_close(int fd) {
    if (fd < 0) return false;
    return close(fd) != -1;
}

bool ipc_shared_memory_remove(void) {
    return shm_unlink(IPC_SHM_NAME) != -1;
}

bool ipc_shared_memory_open(int *fd, size_t *actual_size) {
    if (fd == NULL || actual_size == NULL) return false;
    int opened_fd = shm_open(IPC_SHM_NAME, O_RDWR, 0);
    if (opened_fd == -1) return false;

    struct stat info;
    if (fstat(opened_fd, &info) == -1 || info.st_size <= 0 || (uintmax_t)info.st_size > SIZE_MAX) {
        close(opened_fd);
        return false;
    }

    *fd = opened_fd;
    *actual_size = (size_t)info.st_size;
    return true;
}

bool ipc_shared_memory_create(size_t size, int *fd) {
    if (fd == NULL) return false;
    if (size < sizeof(shared_header_t)) return false;
    if (size > (size_t)INT64_MAX) return false;

    int created_fd = shm_open(IPC_SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0660);
    if (created_fd == -1) return false;
    if (ftruncate(created_fd, (off_t)size) == -1) {
        close(created_fd);
        shm_unlink(IPC_SHM_NAME);
        return false;
    }

    *fd = created_fd;
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
