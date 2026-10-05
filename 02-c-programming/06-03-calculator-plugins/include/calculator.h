#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "plugin.h"

#define TOKEN_ARR_SIZE 100
#define MAX_OPERATIONS 10

//typedef double (*BinaryOperation)(double, double);

typedef struct {
    const OperationPlugin *plugin;
    void *library_handle;
} LoadedOperation;

typedef struct {
    LoadedOperation items[MAX_OPERATIONS];
    size_t size;
} OperationRegistry;

typedef struct{
    double ans;
    bool has_result;
    OperationRegistry operations;
} Calculator;

bool calculator_load_plugins(Calculator *calc, const char *directory_path);

void calculator_unload_plugins(Calculator *calc);

typedef enum {
    CALC_OK,
    CALC_EXIT,
    CALC_INVALID_INPUT,
    CALC_DIVISION_BY_ZERO,
    CALC_TOO_MANY_TOKENS
} CalcStatus;

typedef enum {
    TOKEN_NUMBER,
    TOKEN_OPERATOR,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_ANS
} TokenType;

typedef struct {
    TokenType type;
    union{
        double number;
        char operator;
    } value;
} Token;

typedef struct{
    Token items[TOKEN_ARR_SIZE];
    size_t size;
} TokenArray;


CalcStatus process_command(const char *input, Calculator *calc);
