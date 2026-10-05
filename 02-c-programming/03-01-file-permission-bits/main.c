#include "file_access_rights_via_mask.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    INPUT_SIZE = 256
};

static char *trim(char *input)
{
    while (isspace((unsigned char)*input)) {
        ++input;
    }

    char *end = input + strlen(input);
    while (end > input && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';

    return input;
}

static const char *command_argument(const char *input, const char *command)
{
    size_t command_length = strlen(command);

    if (strncmp(input, command, command_length) != 0 ||
        !isspace((unsigned char)input[command_length])) {
        return NULL;
    }

    const char *argument = input + command_length;
    while (isspace((unsigned char)*argument)) {
        ++argument;
    }

    return argument;
}

static void print_help(void)
{
    puts("Команды:");
    puts("  rights <права>   ввести права: rights 754 или rights rwxr-xr--");
    puts("  file <путь>      получить права файла с помощью stat");
    puts("  chmod <изменение> изменить текущие права: chmod u+x,g-w");
    puts("  show             показать текущие права");
    puts("  help             показать эту справку");
    puts("  exit             завершить программу");
}

static void print_rights(const FileAccessRights *rights)
{
    printf("Буквенное: %s\n", rights->text_view);
    printf("Цифровое:  %s\n", rights->num_view);
    printf("Битовое:   %s\n", rights->bit_view);
}

static void print_status_error(ExecutionStatus status)
{
    if (status == EXECUTION_BAD_INPUT) {
        fprintf(stderr, "Ошибка: некорректный ввод.\n");
    } else {
        fprintf(stderr, "Ошибка: операцию выполнить не удалось.\n");
    }
}

int main(void)
{
    FileAccessRights rights = {0};
    bool rights_are_set = false;
    char buffer[INPUT_SIZE];

    puts("Расчёт маски прав доступа. Для справки введите help.");

    while (true) {
        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            putchar('\n');
            break;
        }

        if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
            int character;
            while ((character = getchar()) != '\n' && character != EOF) {
            }
            fprintf(stderr, "Ошибка: команда слишком длинная.\n");
            continue;
        }

        char *input = trim(buffer);
        if (*input == '\0') {
            continue;
        }

        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }

        if (strcmp(input, "help") == 0) {
            print_help();
            continue;
        }

        if (strcmp(input, "show") == 0) {
            if (!rights_are_set) {
                fprintf(stderr, "Сначала введите права или имя файла.\n");
            } else {
                print_rights(&rights);
            }
            continue;
        }

        const char *argument = command_argument(input, "rights");
        if (argument != NULL) {
            ExecutionStatus status = parse_access_rights(argument, &rights);

            if (status == EXECUTION_OK) {
                rights_are_set = true;
                print_rights(&rights);
            } else {
                print_status_error(status);
            }
            continue;
        }

        argument = command_argument(input, "file");
        if (argument != NULL) {
            if (*argument == '\0') {
                fprintf(stderr, "Ошибка: не указан путь к файлу.\n");
                continue;
            }

            ExecutionStatus status = access_rights_at_file(argument, &rights);

            if (status == EXECUTION_OK) {
                rights_are_set = true;
                printf("Файл:       %s\n", rights.file_name);
                print_rights(&rights);
            } else {
                print_status_error(status);
            }
            continue;
        }

        argument = command_argument(input, "chmod");
        if (argument != NULL) {
            if (!rights_are_set) {
                fprintf(stderr, "Сначала введите права или имя файла.\n");
                continue;
            }

            ExecutionStatus status = change_access_rights(argument, &rights);

            if (status == EXECUTION_OK) {
                print_rights(&rights);
            } else {
                print_status_error(status);
            }
            continue;
        }

        fprintf(stderr, "Неизвестная команда. Введите help для справки.\n");
    }

    return 0;
}
