#include "plugin.h"

static double diff(double left, double right)
{
    return left - right;
}

const OperationPlugin calc_plugin = {
    .symbol = '-',
    .name = "вычитание",
    .priority = 1,
    .right_associative = false,
    .function = diff
};