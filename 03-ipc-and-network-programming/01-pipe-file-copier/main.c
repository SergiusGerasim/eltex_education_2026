#include "copy_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    /*
    разобрать аргументы
    создать каналы
    fork
    если родитель:
        закрыть ненужные концы
        передавать файлы
        сообщить о завершении
        waitpid
        удалить FIFO
    если ребёнок:
        закрыть ненужные концы
        принимать файлы
        завершиться
    */
    if (argc < 2) { 
        fprintf(stderr, "Usage: %s [-p fifo_name] file...\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    char *fifo_name = NULL;

    int first_file_index = 1;
    if (strcmp(argv[1], "-p") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s -p fifo_name file...\n", argv[0]);
            return EXIT_FAILURE;
        }

        fifo_name = argv[2];
        first_file_index = 3;
    }

    bool use_fifo = fifo_name != NULL;
    int parent_to_child[2] = {-1, -1};
    int child_to_parent[2] = {-1, -1};

    if (use_fifo) {
        if (create_fifo_channels(fifo_name) != EXIT_SUCCESS) return EXIT_FAILURE;
    } else {
        if (create_unnamed_channels(parent_to_child, child_to_parent) != EXIT_SUCCESS) return EXIT_FAILURE;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");

        if (use_fifo) {
            remove_fifo_channels(fifo_name);
        } else {
            close(parent_to_child[0]);
            close(parent_to_child[1]);
            close(child_to_parent[0]);
            close(child_to_parent[1]);
        }

        return EXIT_FAILURE;
    }
    
    if (pid == 0){
        // дочерний
        channel_pair_t channels = {.read_fd = -1, .write_fd = -1};

        if (use_fifo) {
            if (open_child_fifo_channels(fifo_name, &channels) != EXIT_SUCCESS) return EXIT_FAILURE;
        } else {
            close(parent_to_child[1]); // ребенок сюда не пишет.
            close(child_to_parent[0]); // не читает то, что пишет родителю.
            channels.read_fd = parent_to_child[0];
            channels.write_fd = child_to_parent[1];
        }

        message_header_t ready_message = {.type = MSG_READY, .file_size = 0, .name_size = 0};

        if (write_full(channels.write_fd, &ready_message, sizeof(ready_message)) == -1) {
            perror("child write");
            close_channels(&channels);
            return EXIT_FAILURE;
        }

        int child_result = EXIT_SUCCESS;

        for (;;) {
            message_header_t message;
            ssize_t result = read_full(channels.read_fd, &message, sizeof(message));

            if (result == -1) {
                perror("child read message header");
                child_result = EXIT_FAILURE;
                break;
            }

            if (result != (ssize_t)sizeof(message)) {
                fprintf(stderr, "Incomplete message header\n");
                child_result = EXIT_FAILURE;
                break;
            }

            if (message.type == MSG_FINISH) {
                printf("Received MSG_FINISH from parent\n");
                break;
            }

            if (message.type != MSG_FILE) {
                fprintf(stderr, "Expected MSG_FILE or MSG_FINISH, received message type %d\n", message.type);
                child_result = EXIT_FAILURE;
                break;
            }

            if (receive_file(channels.read_fd, &message) != EXIT_SUCCESS) {
                child_result = EXIT_FAILURE;
                break;
            }
        }

        close_channels(&channels);
        return child_result;

    } else {
        // родительский
        channel_pair_t channels = {.read_fd = -1, .write_fd = -1};

        if (use_fifo) {
            if (open_parent_fifo_channels(fifo_name, &channels) != EXIT_SUCCESS) {
                remove_fifo_channels(fifo_name);
                return EXIT_FAILURE;
            }
        } else {
            close(parent_to_child[0]);
            close(child_to_parent[1]);
            channels.read_fd = child_to_parent[0];
            channels.write_fd = parent_to_child[1];
        }

        int parent_result = EXIT_SUCCESS;
        message_header_t message;
        ssize_t result = read_full(channels.read_fd, &message, sizeof(message));

        if (result == -1) {
            perror("read ready message");
            parent_result = EXIT_FAILURE;
            goto parent_cleanup;
        }

        if (result != (ssize_t)sizeof(message)) {
            fprintf(stderr, "Incomplete ready message\n");
            parent_result = EXIT_FAILURE;
            goto parent_cleanup;
        }

        if (message.type != MSG_READY) {
            fprintf(stderr, "Expected MSG_READY, received message type %d\n", message.type);
            parent_result = EXIT_FAILURE;
            goto parent_cleanup;
        }

        printf("Received MSG_READY from child\n");

        for (int i = first_file_index; i < argc; i++) {
            send_file_result_t send_result = send_file(channels.write_fd, argv[i]);

            if (send_result == SEND_FILE_SKIPPED) {
                parent_result = EXIT_FAILURE;
                continue;
            }

            if (send_result == SEND_FILE_FATAL) {
                parent_result = EXIT_FAILURE;
                goto parent_cleanup;
            }
        }

        message_header_t finish_message = {
            .type = MSG_FINISH,
            .file_size = 0,
            .name_size = 0
        };

        if (write_full(channels.write_fd, &finish_message, sizeof(finish_message)) == -1) {
            perror("parent write finish message");
            parent_result = EXIT_FAILURE;
        }

parent_cleanup:
        close_channels(&channels);

        int child_status;

        if (waitpid(pid, &child_status, 0) == -1) {
            perror("waitpid");
            parent_result = EXIT_FAILURE;
        } else if (WIFEXITED(child_status)) {
            int exit_status = WEXITSTATUS(child_status);

            if (exit_status != EXIT_SUCCESS) {
                fprintf(stderr, "Child exited with status %d\n", exit_status);
                parent_result = EXIT_FAILURE;
            }
        } else if (WIFSIGNALED(child_status)) {
            fprintf(stderr, "Child terminated by signal %d\n", WTERMSIG(child_status));
            parent_result = EXIT_FAILURE;
        }

        if (use_fifo && remove_fifo_channels(fifo_name) != EXIT_SUCCESS) parent_result = EXIT_FAILURE;

        return parent_result;
    }


    return EXIT_SUCCESS;
}
