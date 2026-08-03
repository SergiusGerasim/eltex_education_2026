#pragma once
#include <stdio.h>
#include <sys/types.h>

typedef struct {
    int read_fd;
    int write_fd;
} channel_pair_t;

int create_unnamed_channels(int parent_to_child[2], int child_to_parent[2]);
int create_fifo_channels(const char *base_name);
int open_parent_fifo_channels(const char *base_name, channel_pair_t *channels);
int open_child_fifo_channels(const char *base_name, channel_pair_t *channels);
void close_channels(channel_pair_t *channels);
int remove_fifo_channels(const char *base_name);

ssize_t write_full(int fd, const void *buffer, size_t size);
ssize_t read_full(int fd, void *buffer, size_t size);