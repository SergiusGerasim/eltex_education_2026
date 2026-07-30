#include "plugin.h"

static double times(double left, double right)
{
    return left * right;
}

const OperationPlugin calc_plugin = {
    .symbol = '*',
    .name = "умножение",
    .priority = 2,
    .right_associative = false,
    .function = times
};