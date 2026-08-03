#include "copy_file.h"
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>

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

    bool bad_file = false;
    for (int i = first_file_index; i < argc; i++) {  
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            perror(argv[i]);
            bad_file = true;
            continue;
        }

        if (close(fd) == -1){
            perror(argv[i]);
            return EXIT_FAILURE;
        }
    }
    if (bad_file) return EXIT_FAILURE;

    int parent_to_child[2];
    int child_to_parent[2];

    if (fifo_name == NULL){
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
    }
    if (fifo_name != NULL) {
        fprintf(stderr, "FIFO mode is not implemented yet\n");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");

        close(parent_to_child[0]);
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        close(child_to_parent[1]);

        return EXIT_FAILURE;
    }
    
    if (pid == 0){
        // дочерний
        close(parent_to_child[1]); // ребенок сюда не пишет.
        close(child_to_parent[0]); // не читает то, что пишет родителю.

        char ready = 'R';

        ssize_t result = write(child_to_parent[1], &ready, sizeof(ready));
        if (result != (ssize_t)sizeof(ready)){
            perror("child write");
            return EXIT_FAILURE;
        }

        close(parent_to_child[0]);
        close(child_to_parent[1]);
        return EXIT_SUCCESS;

    } else {
        // родительский
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        char message;

        ssize_t result = read(child_to_parent[0], &message, sizeof(message));
        if (result == -1){
            perror("parent read");
            close(parent_to_child[1]);
            close(child_to_parent[0]);
            return EXIT_FAILURE;
        }
        if (result == 0){
            fprintf(stderr, "Child closed the channel\n");
            close(parent_to_child[1]);
            close(child_to_parent[0]);
            return EXIT_FAILURE;
        }
        printf("Received from child: %c\n", message);

        // ждёт ребёнка:
        if (waitpid(pid, NULL, 0) == -1){
            perror("waitpid");
            close(parent_to_child[1]);
            close(child_to_parent[0]);
            return EXIT_FAILURE;
        }

        close(parent_to_child[1]);
        close(child_to_parent[0]);

        return EXIT_SUCCESS;
    }


    return EXIT_SUCCESS;
}