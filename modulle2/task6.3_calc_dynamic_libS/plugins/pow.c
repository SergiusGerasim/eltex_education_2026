#include "plugin.h"

#include <math.h>

static double power(double base, double exponent)
{
    return pow(base, exponent);
}

const OperationPlugin calc_plugin = {
    .symbol = '^',
    .name = "возведение в степень",
    .priority = 3,
    .right_associative = true,
    .function = power
};