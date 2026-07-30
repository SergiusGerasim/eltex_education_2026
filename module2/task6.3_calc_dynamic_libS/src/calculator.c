#include "calculator.h"

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const OperationPlugin *get_operation(const Calculator *calc, char symbol){
    for (size_t i = 0; i < calc->operations.size; ++i) {
        const OperationPlugin *plugin = calc->operations.items[i].plugin;

        if (plugin != NULL && plugin->symbol == symbol) return plugin;
    }

    return NULL;
}

static bool has_so_extension(const char *name){
    size_t length = strlen(name);
    return length >= 3 && strcmp(name + length - 3, ".so") == 0;
}

static bool load_plugin(Calculator *calc, const char *path){
    if (calc->operations.size >= MAX_OPERATIONS) {
        fprintf(stderr,"Не удалось загрузить %s: достигнут предел операций.\n",path);
        return false;
    }

    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        fprintf(stderr, "Не удалось загрузить %s: %s\n", path, dlerror());
        return false;
    }

    dlerror();
    const OperationPlugin *plugin = dlsym(handle, "calc_plugin");
    const char *error = dlerror();

    if (error != NULL) {
        fprintf(stderr, "В библиотеке %s не найден calc_plugin: %s\n",path,error);
        dlclose(handle);
        return false;
    }

    if (plugin->function == NULL || plugin->name == NULL) {
        fprintf(stderr, "Библиотека %s содержит некорректный плагин.\n", path);
        dlclose(handle);
        return false;
    }

    if (get_operation(calc, plugin->symbol) != NULL) {
        fprintf(stderr,"Операция '%c' из %s уже загружена.\n",plugin->symbol,path);
        dlclose(handle);
        return false;
    }

    LoadedOperation *loaded = &calc->operations.items[calc->operations.size++];
    loaded->plugin = plugin;
    loaded->library_handle = handle;

    return true;
}

bool calculator_load_plugins(Calculator *calc, const char *directory_path)
{
    if (calc == NULL || directory_path == NULL) return false;

    DIR *directory = opendir(directory_path);
    if (directory == NULL) {
        fprintf(stderr,"Не удалось открыть каталог %s: %s\n",directory_path,strerror(errno));
        return false;
    }

    bool loaded_any = false;
    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {
        if (!has_so_extension(entry->d_name)) continue;


        size_t directory_length = strlen(directory_path);
        size_t name_length = strlen(entry->d_name);
        bool needs_separator = directory_length != 0 && directory_path[directory_length - 1] != '/';
        size_t path_size = directory_length + (needs_separator ? 1U : 0U) + name_length + 1U;
        char *path = malloc(path_size);

        if (path == NULL) {
            fprintf(stderr, "Не удалось выделить память для пути к плагину.\n");
            continue;
        }

        snprintf(path,path_size,needs_separator ? "%s/%s" : "%s%s",directory_path,entry->d_name);

        if (load_plugin(calc, path)) loaded_any = true;

        free(path);
    }

    if (closedir(directory) != 0) {
        fprintf(stderr,"Не удалось закрыть каталог %s: %s\n",directory_path,strerror(errno));
    }

    return loaded_any;
}

void calculator_unload_plugins(Calculator *calc){
    if (calc == NULL) return;

    for (size_t i = 0; i < calc->operations.size; ++i) {
        LoadedOperation *loaded = &calc->operations.items[i];

        if (loaded->library_handle != NULL) dlclose(loaded->library_handle);

        loaded->plugin = NULL;
        loaded->library_handle = NULL;
    }

    calc->operations.size = 0;
}

static int priority(const Calculator *calc, char symbol){
    const OperationPlugin *plugin = get_operation(calc, symbol);
    return plugin == NULL ? -1 : plugin->priority;
}

static bool is_right_associative(const Calculator *calc, char symbol){
    const OperationPlugin *plugin = get_operation(calc, symbol);
    return plugin != NULL && plugin->right_associative;
}

static void print_help(const Calculator *calc){
    printf("Доступные операции:\n");

    for (size_t i = 0; i < calc->operations.size; ++i) {
        const OperationPlugin *plugin = calc->operations.items[i].plugin;
        printf("  %c  %s\n", plugin->symbol, plugin->name);
    }

    printf(
        "\n"
        "Поддерживаются числа, скобки и переменная ans.\n"
        "ans содержит результат последнего успешного вычисления.\n"
        "\n"
        "Примеры:\n"
        "  2 + 3 * 4\n"
        "  (2 + 3) * 4\n"
        "  ans / 2\n"
        "\n"
        "Команды:\n"
        "  help   показать эту справку\n"
        "  clear  очистить сохранённый результат\n"
        "  exit   завершить работу\n"
    );
}

static CalcStatus append_token(TokenArray *array, Token token){
    if (array->size >= TOKEN_ARR_SIZE) return CALC_TOO_MANY_TOKENS;

    array->items[array->size++] = token;
    return CALC_OK;
}

static bool is_operator(const Calculator *calc, char symbol){
    return get_operation(calc, symbol) != NULL;
}

static CalcStatus tokenize(const char *input,const Calculator *calc,TokenArray *result){
    const char *current = input;

    while (*current != '\0') {
        if (isspace((unsigned char)*current)) ++current;
        else if (isdigit((unsigned char)*current) || *current == '.') {
            char *end;
            errno = 0;
            double number = strtod(current, &end);

            if (end == current || errno == ERANGE) return CALC_INVALID_INPUT;

            Token token = {
                .type = TOKEN_NUMBER,
                .value.number = number
            };
            CalcStatus status = append_token(result, token);

            if (status != CALC_OK) return status;

            current = end;
        }
        else if (is_operator(calc, *current)) {
            Token token = {
                .type = TOKEN_OPERATOR,
                .value.operator = *current
            };
            CalcStatus status = append_token(result, token);
            
            if (status != CALC_OK) return status;

            ++current;
        }
        else if (*current == '(') {
            Token token = {.type = TOKEN_LEFT_PAREN};
            CalcStatus status = append_token(result, token);

            if (status != CALC_OK) return status;

            ++current;
        }
        else if (*current == ')') {
            Token token = {.type = TOKEN_RIGHT_PAREN};
            CalcStatus status = append_token(result, token);

            if (status != CALC_OK) return status;

            ++current;
        }
        else if (strncmp(current, "ans", 3) == 0 &&
                 !isalnum((unsigned char)current[3]) &&
                 current[3] != '_') {
            Token token = {.type = TOKEN_ANS};
            CalcStatus status = append_token(result, token);

            if (status != CALC_OK) return status;

            current += 3;
        }
        else return CALC_INVALID_INPUT;
    }

    return result->size == 0 ? CALC_INVALID_INPUT : CALC_OK;
}

static CalcStatus to_postfix(const Calculator *calc,const TokenArray *infix,TokenArray *postfix){
    TokenArray stack = {0};

    for (size_t i = 0; i < infix->size; ++i) {
        Token current = infix->items[i];
        CalcStatus status;

        if (current.type == TOKEN_NUMBER || current.type == TOKEN_ANS) {
            status = append_token(postfix, current);
            if (status != CALC_OK) return status;
        }
        else if (current.type == TOKEN_LEFT_PAREN) {
            status = append_token(&stack, current);
            if (status != CALC_OK) return status;
        }
        else if (current.type == TOKEN_OPERATOR) {
            while (stack.size != 0 &&
                   stack.items[stack.size - 1].type == TOKEN_OPERATOR) {
                char top_symbol =
                    stack.items[stack.size - 1].value.operator;
                char current_symbol = current.value.operator;
                bool top_goes_first =
                    priority(calc, top_symbol) >
                        priority(calc, current_symbol) ||
                    (priority(calc, top_symbol) ==
                         priority(calc, current_symbol) &&
                     !is_right_associative(calc, current_symbol));

                if (!top_goes_first) {
                    break;
                }

                status = append_token(postfix, stack.items[--stack.size]);
                if (status != CALC_OK) return status;
            }

            status = append_token(&stack, current);
            if (status != CALC_OK) return status;
        }
        else if (current.type == TOKEN_RIGHT_PAREN) {
            while (stack.size != 0 &&
                   stack.items[stack.size - 1].type != TOKEN_LEFT_PAREN) {
                status = append_token(postfix, stack.items[--stack.size]);
                if (status != CALC_OK) return status;
            }

            if (stack.size == 0) return CALC_INVALID_INPUT;

            --stack.size;
        }
        else return CALC_INVALID_INPUT;
    }

    while (stack.size != 0) {
        Token top = stack.items[--stack.size];

        if (top.type == TOKEN_LEFT_PAREN ||
            top.type == TOKEN_RIGHT_PAREN) {
            return CALC_INVALID_INPUT;
        }

        CalcStatus status = append_token(postfix, top);
        if (status != CALC_OK) return status;
    }

    return CALC_OK;
}

static CalcStatus calculate_postfix(Calculator *calc, const TokenArray *postfix){
    double stack[TOKEN_ARR_SIZE] = {0};
    size_t stack_size = 0;

    for (size_t i = 0; i < postfix->size; ++i) {
        Token current = postfix->items[i];

        if (current.type == TOKEN_ANS) {
            if (!calc->has_result) return CALC_INVALID_INPUT;

            if (stack_size >= TOKEN_ARR_SIZE) return CALC_TOO_MANY_TOKENS;

            stack[stack_size++] = calc->ans;
        }
        else if (current.type == TOKEN_NUMBER) {
            if (stack_size >= TOKEN_ARR_SIZE) return CALC_TOO_MANY_TOKENS;

            stack[stack_size++] = current.value.number;
        }
        else if (current.type == TOKEN_OPERATOR) {
            if (stack_size < 2) return CALC_INVALID_INPUT;

            double right = stack[--stack_size];
            double left = stack[--stack_size];
            char symbol = current.value.operator;

            if (symbol == '/' && right == 0.0) return CALC_DIVISION_BY_ZERO;

            const OperationPlugin *operation = get_operation(calc, symbol);
            if (operation == NULL) return CALC_INVALID_INPUT;

            stack[stack_size++] = operation->function(left, right);
        }
        else return CALC_INVALID_INPUT;
    }

    if (stack_size != 1) return CALC_INVALID_INPUT;

    calc->ans = stack[0];
    calc->has_result = true;
    return CALC_OK;
}

static CalcStatus calculate_expression(const char *input, Calculator *calc){
    TokenArray tokens = {0};
    CalcStatus status = tokenize(input, calc, &tokens);

    if (status != CALC_OK) return status;

    TokenArray postfix = {0};
    status = to_postfix(calc, &tokens, &postfix);

    if (status != CALC_OK) return status;

    return calculate_postfix(calc, &postfix);
}

CalcStatus process_command(const char *input, Calculator *calc){
    if (strcmp(input, "exit") == 0) return CALC_EXIT;

    if (strcmp(input, "clear") == 0) {
        calc->ans = 0.0;
        calc->has_result = false;
        return CALC_OK;
    }

    if (strcmp(input, "help") == 0) {
        print_help(calc);
        return CALC_OK;
    }

    return calculate_expression(input, calc);
}
