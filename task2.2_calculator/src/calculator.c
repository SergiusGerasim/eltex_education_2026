#include "calculator.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

static double sum(double x1, double x2){
    return x1 + x2;
}

static double diff(double x1, double x2){
    return x1 - x2;
}

static double times(double x1, double x2){
    return x1 * x2;
}

static double divide(double x1, double x2){
    //if (x2 == 0) return NAN;
    return x1 / x2;
}

static BinaryOperation get_operation(char operator)
{
    switch (operator) {
    case '+': return sum;
    case '-': return diff;
    case '*': return times;
    case '/': return divide;
    case '^': return pow;
    default:  return NULL;
    }
}

static int priority(char oper){
    switch (oper){
        case '+': 
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
        case '^':
            return 3;
        default:
            return -1;
    }
}

static bool is_right_associative(char operator)
{
    return operator == '^';
}

static void print_help(void){
    printf(
        "Доступные операции:\n"
        "  +  сложение\n"
        "  -  вычитание\n"
        "  *  умножение\n"
        "  /  деление\n"
        "  ^  возведение в степень\n"
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

static CalcStatus append_token(TokenArray *array, Token token)
{
    if (array->size >= TOKEN_ARR_SIZE) {
        return CALC_TOO_MANY_TOKENS;
    }

    array->items[array->size++] = token;
    return CALC_OK;
}

static bool is_operator(char oper){
    if (oper == '+' || oper == '-' || oper == '/' || oper == '*' || oper == '^') return true;
    return false;
}

static CalcStatus tokenize(const char *input, TokenArray *res){
    const char *current = input;
    while(*current != '\0'){
        if (isspace((unsigned char)*current)) ++current;
        else if (isdigit((unsigned char)*current) || *current == '.') {
            char *end;
            errno = 0;
            double num = strtod(current, &end);
            if (end == current || errno == ERANGE) return CALC_INVALID_INPUT;


            Token token = {.type = TOKEN_NUMBER, .value.number = num};
            CalcStatus status = append_token(res, token);

            if (status != CALC_OK) {
                return status;
            }

            current = end;
        }
        else if (is_operator(*current)){
            Token token = {.type = TOKEN_OPERATOR, .value.operator = *current};
            CalcStatus status = append_token(res, token);
            if (status != CALC_OK) {
                return status;
            }
            ++current;
        }
        else if (*current == '('){
            Token token = {.type = TOKEN_LEFT_PAREN};
            CalcStatus status = append_token(res, token);
            if (status != CALC_OK) {
                return status;
            }
            ++current;
        } 
        else if (*current == ')'){
            Token token = {.type = TOKEN_RIGHT_PAREN};
            CalcStatus status = append_token(res, token);
            if (status != CALC_OK) {
                return status;
            }
            ++current;
        }
        else if (strncmp(current, "ans", 3) == 0 && !isalnum((unsigned char)current[3]) && current[3] != '_'){
            Token token = {.type = TOKEN_ANS};
            CalcStatus status = append_token(res, token);
            if (status != CALC_OK) {
                return status;
            }
            current += 3;
        }
        else {
            return CALC_INVALID_INPUT;
        }
    }
    if (res->size == 0) return CALC_INVALID_INPUT;
    return CALC_OK;
}

static CalcStatus to_postfix(const TokenArray *infix, TokenArray *postfix){
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
                char top_operator = stack.items[stack.size - 1].value.operator;
                char current_operator = current.value.operator;
                bool top_goes_first =
                    priority(top_operator) > priority(current_operator) ||
                    (priority(top_operator) == priority(current_operator) &&
                     !is_right_associative(current_operator));

                if (!top_goes_first) break;

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
        else {
            return CALC_INVALID_INPUT;
        }
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
            if (!calc->has_result) {
                return CALC_INVALID_INPUT;
            }

            if (stack_size >= TOKEN_ARR_SIZE) {
                return CALC_TOO_MANY_TOKENS;
            }

            stack[stack_size++] = calc->ans;
        }
        else if (current.type == TOKEN_NUMBER) {
            if (stack_size >= TOKEN_ARR_SIZE) {
                return CALC_TOO_MANY_TOKENS;
            }

            stack[stack_size++] = current.value.number;
        }
        else if (current.type == TOKEN_OPERATOR) {
            if (stack_size < 2) {
                return CALC_INVALID_INPUT;
            }

            double right = stack[--stack_size];
            double left = stack[--stack_size];
            char operator = current.value.operator;

            if (operator == '/' && right == 0.0) {
                return CALC_DIVISION_BY_ZERO;
            }

            BinaryOperation operation = get_operation(operator);
            if (operation == NULL) {
                return CALC_INVALID_INPUT;
            }

            stack[stack_size++] = operation(left, right);
        }
        else {
            return CALC_INVALID_INPUT;
        }
    }

    if (stack_size != 1) {
        return CALC_INVALID_INPUT;
    }

    calc->ans = stack[0];
    calc->has_result = true;
    return CALC_OK;
}

static CalcStatus calculate_expression(const char *input, Calculator *calc){
    // предпологается что в передаваемом calc хранится результат предыдущих вычислений
    TokenArray tokens = {0};
    CalcStatus status = tokenize(input, &tokens);
    if (status != CALC_OK) return status;

    TokenArray postfix = {0};
    status = to_postfix(&tokens, &postfix);
    if (status != CALC_OK) return status;

    status = calculate_postfix(calc, &postfix);
    return status;
}

CalcStatus process_command(const char *input, Calculator *calc){
    if (strcmp(input, "exit") == 0) return CALC_EXIT;
    if (strcmp(input, "clear") == 0){
        calc->ans = 0;
        calc->has_result = false;
        return CALC_OK;
    }
    if (strcmp(input, "help") == 0){
        print_help();
        return CALC_OK;
    }
    else{
        return calculate_expression(input, calc);
    }
}
