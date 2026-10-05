#include "calculator.h"

#include <ctype.h>
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

static void print_error(CalcStatus status)
{
    switch (status) {
    case CALC_INVALID_INPUT:
        fprintf(stderr, "Ошибка: некорректное выражение.\n");
        break;
    case CALC_DIVISION_BY_ZERO:
        fprintf(stderr, "Ошибка: деление на ноль.\n");
        break;
    case CALC_TOO_MANY_TOKENS:
        fprintf(stderr, "Ошибка: выражение слишком длинное.\n");
        break;
    case CALC_OK:
    case CALC_EXIT:
        break;
    }
}

int main(void)
{
    Calculator calc = {
        .ans = 0.0,
        .has_result = false
    };
    char buffer[INPUT_SIZE];

    printf("Умный калькулятор. Для справки введите help.\n");

    while (true) {
        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            putchar('\n');
            break;
        }

        char *input = trim(buffer);
        if (*input == '\0') {
            continue;
        }

        CalcStatus status = process_command(input, &calc);

        if (status == CALC_EXIT) {
            break;
        }

        if (status != CALC_OK) {
            print_error(status);
            continue;
        }

        if (strcmp(input, "clear") == 0) {
            printf("Результат очищен.\n");
        }
        else if (strcmp(input, "help") != 0) {
            printf("= %.15g\n", calc.ans);
        }
    }

    return 0;
}
