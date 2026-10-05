#include "copy_file.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *make_fifo_path(const char *base_name, const char *suffix) {
    size_t base_size = strlen(base_name);
    size_t suffix_size = strlen(suffix);

    if (base_size > SIZE_MAX - suffix_size - 1) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char *path = malloc(base_size + suffix_size + 1);

    if (path == NULL) return NULL;

    memcpy(path, base_name, base_size);
    memcpy(path + base_size, suffix, suffix_size + 1);
    return path;
}

static int open_fifo(const char *path, int flags) {
    int fd;

    do {
        fd = open(path, flags);
    } while (fd == -1 && errno == EINTR);

    return fd;
}

ssize_t write_full(int fd, const void *buffer, size_t size){
    const unsigned char *data = buffer;
    size_t total_written = 0;

    while (total_written < size){
        ssize_t result = write(fd, data + total_written, size - total_written);
        if (result == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (result == 0){
            errno = EIO;
            return -1;
        }

        total_written += (size_t)result;
    }
    return (ssize_t)total_written;
}

ssize_t read_full(int fd, void *buffer, size_t size){
    unsigned char *data = buffer;
    size_t total_read = 0;
    while (total_read < size){
        ssize_t result = read(fd, data + total_read, size - total_read);
        if (result == -1){
            if (errno == EINTR) continue;
            return -1;
        }
        if (result == 0) break;

        total_read += (size_t)result;
    }

    return (ssize_t)total_read;
}



int create_unnamed_channels(int parent_to_child[2], int child_to_parent[2]){
    if(pipe(parent_to_child) == -1){
        perror("pipe parent_to_child");
        return EXIT_FAILURE;
    }
    if (pipe(child_to_parent) == -1) {
        perror("pipe child_to_parent");
        close(parent_to_child[0]);
        close(parent_to_child[1]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int create_fifo_channels(const char *base_name) {
    char *data_path = make_fifo_path(base_name, ".data");
    char *ready_path = make_fifo_path(base_name, ".ready");

    if (data_path == NULL || ready_path == NULL) {
        perror("create FIFO path");
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    if (mkfifo(data_path, 0600) == -1) {
        perror(data_path);
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    if (mkfifo(ready_path, 0600) == -1) {
        perror(ready_path);
        if (unlink(data_path) == -1) perror(data_path);
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    free(data_path);
    free(ready_path);
    return EXIT_SUCCESS;
}

int open_parent_fifo_channels(const char *base_name, channel_pair_t *channels) {
    channels->read_fd = -1;
    channels->write_fd = -1;

    char *data_path = make_fifo_path(base_name, ".data");
    char *ready_path = make_fifo_path(base_name, ".ready");

    if (data_path == NULL || ready_path == NULL) {
        perror("create FIFO path");
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    channels->write_fd = open_fifo(data_path, O_WRONLY);

    if (channels->write_fd == -1) {
        perror(data_path);
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    channels->read_fd = open_fifo(ready_path, O_RDONLY);

    if (channels->read_fd == -1) {
        perror(ready_path);
        close(channels->write_fd);
        channels->write_fd = -1;
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    free(data_path);
    free(ready_path);
    return EXIT_SUCCESS;
}

int open_child_fifo_channels(const char *base_name, channel_pair_t *channels) {
    channels->read_fd = -1;
    channels->write_fd = -1;

    char *data_path = make_fifo_path(base_name, ".data");
    char *ready_path = make_fifo_path(base_name, ".ready");

    if (data_path == NULL || ready_path == NULL) {
        perror("create FIFO path");
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    channels->read_fd = open_fifo(data_path, O_RDONLY);

    if (channels->read_fd == -1) {
        perror(data_path);
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    channels->write_fd = open_fifo(ready_path, O_WRONLY);

    if (channels->write_fd == -1) {
        perror(ready_path);
        close(channels->read_fd);
        channels->read_fd = -1;
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    free(data_path);
    free(ready_path);
    return EXIT_SUCCESS;
}

void close_channels(channel_pair_t *channels) {
    if (channels->read_fd >= 0) {
        if (close(channels->read_fd) == -1) perror("close channel for reading");
        channels->read_fd = -1;
    }

    if (channels->write_fd >= 0) {
        if (close(channels->write_fd) == -1) perror("close channel for writing");
        channels->write_fd = -1;
    }
}

int remove_fifo_channels(const char *base_name) {
    char *data_path = make_fifo_path(base_name, ".data");
    char *ready_path = make_fifo_path(base_name, ".ready");

    if (data_path == NULL || ready_path == NULL) {
        perror("create FIFO path");
        free(data_path);
        free(ready_path);
        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;

    if (unlink(data_path) == -1 && errno != ENOENT) {
        perror(data_path);
        status = EXIT_FAILURE;
    }

    if (unlink(ready_path) == -1 && errno != ENOENT) {
        perror(ready_path);
        status = EXIT_FAILURE;
    }

    free(data_path);
    free(ready_path);
    return status;
}

send_file_result_t send_file(int channel_fd, const char *file_name) {
    int input_fd = open(file_name, O_RDONLY);

    if (input_fd == -1) {
        perror(file_name);
        return SEND_FILE_SKIPPED;
    }

    struct stat file_info;

    if (fstat(input_fd, &file_info) == -1) {
        perror("fstat");
        close(input_fd);
        return SEND_FILE_SKIPPED;
    }

    if (file_info.st_size < 0) {
        fprintf(stderr, "Invalid file size: %s\n", file_name);
        close(input_fd);
        return SEND_FILE_SKIPPED;
    }

    size_t name_size = strlen(file_name);

    if (name_size > UINT32_MAX) {
        fprintf(stderr, "File name is too long: %s\n", file_name);
        close(input_fd);
        return SEND_FILE_SKIPPED;
    }

    message_header_t file_message = {
        .type = MSG_FILE,
        .file_size = (uint64_t)file_info.st_size,
        .name_size = (uint32_t)name_size
    };

    if (write_full(channel_fd, &file_message, sizeof(file_message)) == -1) {
        perror("write file header");
        close(input_fd);
        return SEND_FILE_FATAL;
    }

    if (write_full(channel_fd, file_name, name_size) == -1) {
        perror("write file name");
        close(input_fd);
        return SEND_FILE_FATAL;
    }

    unsigned char buffer[COPY_BUFFER_SIZE];
    uint64_t remaining = file_message.file_size;

    while (remaining > 0) {
        size_t chunk_size = sizeof(buffer);

        if (remaining < chunk_size) chunk_size = (size_t)remaining;

        ssize_t bytes_read = read(input_fd, buffer, chunk_size);

        if (bytes_read == -1) {
            if (errno == EINTR) continue;

            perror("read input file");
            close(input_fd);
            return SEND_FILE_FATAL;
        }

        if (bytes_read == 0) {
            fprintf(stderr, "Unexpected end of file: %s\n", file_name);
            close(input_fd);
            return SEND_FILE_FATAL;
        }

        if (write_full(channel_fd, buffer, (size_t)bytes_read) == -1) {
            perror("write file data");
            close(input_fd);
            return SEND_FILE_FATAL;
        }

        remaining -= (uint64_t)bytes_read;
    }

    if (close(input_fd) == -1) {
        perror(file_name);
        return SEND_FILE_FATAL;
    }

    return SEND_FILE_OK;
}

int receive_file(int channel_fd, const message_header_t *file_message) {
    printf("Received MSG_FILE: name_size=%" PRIu32 ", file_size=%" PRIu64 "\n", file_message->name_size, file_message->file_size);

    size_t name_size = (size_t)file_message->name_size;
    char *file_name = malloc(name_size + 1);

    if (file_name == NULL) {
        perror("malloc file name");
        return EXIT_FAILURE;
    }

    ssize_t result = read_full(channel_fd, file_name, name_size);

    if (result == -1) {
        perror("read file name");
        free(file_name);
        return EXIT_FAILURE;
    }

    if (result != (ssize_t)name_size) {
        fprintf(stderr, "Incomplete file name\n");
        free(file_name);
        return EXIT_FAILURE;
    }

    file_name[name_size] = '\0';
    printf("Received file name: %s\n", file_name);

    if (name_size > SIZE_MAX - sizeof(".copy")) {
        fprintf(stderr, "Output file name is too long\n");
        free(file_name);
        return EXIT_FAILURE;
    }

    size_t copy_name_size = name_size + sizeof(".copy");
    char *copy_name = malloc(copy_name_size);

    if (copy_name == NULL) {
        perror("malloc copy name");
        free(file_name);
        return EXIT_FAILURE;
    }

    memcpy(copy_name, file_name, name_size);
    memcpy(copy_name + name_size, ".copy", sizeof(".copy"));
    printf("Output file name: %s\n", copy_name);

    int output_fd = open(copy_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (output_fd == -1) {
        perror(copy_name);
        free(copy_name);
        free(file_name);
        return EXIT_FAILURE;
    }

    unsigned char buffer[COPY_BUFFER_SIZE];
    uint64_t remaining = file_message->file_size;

    while (remaining > 0) {
        size_t chunk_size = sizeof(buffer);

        if (remaining < chunk_size) chunk_size = (size_t)remaining;

        result = read_full(channel_fd, buffer, chunk_size);

        if (result == -1) {
            perror("read file data");
            close(output_fd);
            free(copy_name);
            free(file_name);
            return EXIT_FAILURE;
        }

        if (result != (ssize_t)chunk_size) {
            fprintf(stderr, "Incomplete file data\n");
            close(output_fd);
            free(copy_name);
            free(file_name);
            return EXIT_FAILURE;
        }

        if (write_full(output_fd, buffer, chunk_size) == -1) {
            perror(copy_name);
            close(output_fd);
            free(copy_name);
            free(file_name);
            return EXIT_FAILURE;
        }

        remaining -= chunk_size;
    }

    if (close(output_fd) == -1) {
        perror(copy_name);
        free(copy_name);
        free(file_name);
        return EXIT_FAILURE;
    }

    free(copy_name);
    free(file_name);
    return EXIT_SUCCESS;
}
