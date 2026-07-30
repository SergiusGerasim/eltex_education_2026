#pragma once
#include <stdbool.h>

typedef double (*BinaryOperation)(double, double);

typedef struct {
    char symbol;
    const char *name;
    int priority;
    bool right_associative;
    BinaryOperation function;
} OperationPlugin;
