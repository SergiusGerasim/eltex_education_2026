#pragma once

typedef struct {
    unsigned int read_interval_seconds;
} consumer_config_t;

int consumer_run(const consumer_config_t *config);
