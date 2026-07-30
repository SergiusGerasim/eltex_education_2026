#include "plugin.h"

static double divide(double left, double right)
{
    return left / right;
}

const OperationPlugin calc_plugin = {
    .symbol = '/',
    .name = "деление",
    .priority = 2,
    .right_associative = false,
    .function = divide
};