#include "priority_queue.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define INPUT_SIZE 128

static bool read_line(const char *prompt, char *buffer, size_t buffer_size){
    if (prompt != NULL) printf("%s", prompt);

    if (fgets(buffer, (int)buffer_size, stdin) == NULL) return false;

    size_t length = 0;
    while (buffer[length] != '\0' && buffer[length] != '\n') length++;

    if (buffer[length] == '\n'){
        buffer[length] = '\0';
        return true;
    }

    int symbol = getchar();
    if (symbol == '\n' || symbol == EOF) return true;

    while ((symbol = getchar()) != '\n' && symbol != EOF){}
    return false;
}

static bool read_number(
    const char *prompt,
    size_t minimum,
    size_t maximum,
    size_t *result
){
    char input[INPUT_SIZE];

    if (!read_line(prompt, input, sizeof(input))) return false;

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(input, &end, 10);

    if (
        errno != 0 ||
        end == input ||
        *end != '\0' ||
        value < minimum ||
        value > maximum
    ){
        return false;
    }

    *result = (size_t)value;
    return true;
}

static void print_menu(void){
    printf(
        "\nОчередь с приоритетом (255 — наивысший приоритет)\n"
        "1. Добавить сообщение\n"
        "2. Сгенерировать случайные сообщения\n"
        "3. Посмотреть первое сообщение\n"
        "4. Извлечь первое сообщение\n"
        "5. Извлечь сообщение с указанным приоритетом\n"
        "6. Извлечь сообщение с приоритетом не ниже заданного\n"
        "0. Выход\n"
    );
}

static void add_message(PriorityQueue *queue){
    size_t priority;
    char message[MESSAGE_SIZE];

    if (!read_number("Приоритет (0–255): ", 0, 255, &priority)){
        puts("Некорректный приоритет.");
        return;
    }

    if (!read_line("Текст сообщения: ", message, sizeof(message))){
        printf("Сообщение должно быть короче %d символов.\n", MESSAGE_SIZE);
        return;
    }

    if (message[0] == '\0'){
        puts("Сообщение не должно быть пустым.");
        return;
    }

    if (!push(queue, priority, message)){
        puts("Не удалось добавить сообщение.");
        return;
    }

    puts("Сообщение добавлено.");
}

static void generate_messages(PriorityQueue *queue, unsigned int *next_id){
    size_t count;

    if (!read_number("Количество сообщений (1–10000): ", 1, 10000, &count)){
        puts("Некорректное количество.");
        return;
    }

    size_t added = 0;
    for (size_t i = 0; i < count; i++){
        size_t priority = (size_t)(rand() % PRIORITY_LEVELS);
        char message[MESSAGE_SIZE];

        int written = snprintf(
            message,
            sizeof(message),
            "Случайное сообщение #%u, приоритет %zu",
            *next_id,
            priority
        );

        if (
            written < 0 ||
            (size_t)written >= sizeof(message) ||
            !push(queue, priority, message)
        ){
            break;
        }

        (*next_id)++;
        added++;
    }

    printf("Добавлено сообщений: %zu из %zu.\n", added, count);
}

static void peek_message(const PriorityQueue *queue){
    char message[MESSAGE_SIZE];

    if (peek(queue, message)){
        printf("Первое сообщение: %s\n", message);
    }
    else{
        puts("Очередь пуста.");
    }
}

static void pop_first_message(PriorityQueue *queue){
    char message[MESSAGE_SIZE];

    if (pop_first(queue, message)){
        printf("Извлечено: %s\n", message);
    }
    else{
        puts("Очередь пуста.");
    }
}

static void pop_exact_priority(PriorityQueue *queue){
    size_t priority;
    char message[MESSAGE_SIZE];

    if (!read_number("Приоритет (0–255): ", 0, 255, &priority)){
        puts("Некорректный приоритет.");
        return;
    }

    if (pop_by_priority(queue, priority, message)){
        printf("Извлечено: %s\n", message);
    }
    else{
        puts("Сообщений с таким приоритетом нет.");
    }
}

static void pop_at_least_priority(PriorityQueue *queue){
    size_t priority;
    char message[MESSAGE_SIZE];

    if (!read_number("Минимальный приоритет (0–255): ", 0, 255, &priority)){
        puts("Некорректный приоритет.");
        return;
    }

    if (pop_by_priority_or_upper(queue, priority, message)){
        printf("Извлечено: %s\n", message);
    }
    else{
        puts("Сообщений с подходящим приоритетом нет.");
    }
}

int main(void){
    PriorityQueue queue;
    if (!priority_queue_init(&queue)){
        fputs("Не удалось инициализировать очередь.\n", stderr);
        return EXIT_FAILURE;
    }

    srand((unsigned int)time(NULL));
    unsigned int next_id = 1;
    bool running = true;

    while (running){
        size_t command;
        print_menu();

        if (!read_number("Выберите действие: ", 0, 6, &command)){
            if (feof(stdin)){
                putchar('\n');
                break;
            }
            puts("Неизвестная команда.");
            continue;
        }

        switch (command){
            case 0:
                running = false;
                break;
            case 1:
                add_message(&queue);
                break;
            case 2:
                generate_messages(&queue, &next_id);
                break;
            case 3:
                peek_message(&queue);
                break;
            case 4:
                pop_first_message(&queue);
                break;
            case 5:
                pop_exact_priority(&queue);
                break;
            case 6:
                pop_at_least_priority(&queue);
                break;
            default:
                break;
        }
    }

    priority_queue_free(&queue);
    puts("Работа завершена.");
    return EXIT_SUCCESS;
}
