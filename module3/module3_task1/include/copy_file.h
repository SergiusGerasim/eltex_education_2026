#pragma once
#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>

#define COPY_BUFFER_SIZE 4096

typedef struct {
    int read_fd;
    int write_fd;
} channel_pair_t;

typedef enum {
    MSG_READY,
    MSG_FILE,
    MSG_FINISH
} message_type_t;

typedef enum {
    SEND_FILE_OK,
    SEND_FILE_SKIPPED,
    SEND_FILE_FATAL
} send_file_result_t;

typedef struct {
    message_type_t type;
    uint64_t file_size;
    uint32_t name_size;
} message_header_t;

int create_unnamed_channels(int parent_to_child[2], int child_to_parent[2]);
int create_fifo_channels(const char *base_name);
int open_parent_fifo_channels(const char *base_name, channel_pair_t *channels);
int open_child_fifo_channels(const char *base_name, channel_pair_t *channels);
void close_channels(channel_pair_t *channels);
int remove_fifo_channels(const char *base_name);

ssize_t write_full(int fd, const void *buffer, size_t size);
ssize_t read_full(int fd, void *buffer, size_t size);
send_file_result_t send_file(int channel_fd, const char *file_name);
int receive_file(int channel_fd, const message_header_t *file_message);
