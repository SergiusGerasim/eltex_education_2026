#include "plugin.h"

static double add(double left, double right)
{
    return left + right;
}

const OperationPlugin calc_plugin = {
    .symbol = '+',
    .name = "сложение",
    .priority = 1,
    .right_associative = false,
    .function = add
};