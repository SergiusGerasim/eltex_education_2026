#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t segment_size;
    uint32_t min_array_length;
    uint32_t max_array_length;
    int32_t min_value;
    int32_t max_value;
    unsigned int generation_interval_seconds;
    unsigned int check_interval_seconds;
} producer_config_t;

int producer_run(const producer_config_t *config);
