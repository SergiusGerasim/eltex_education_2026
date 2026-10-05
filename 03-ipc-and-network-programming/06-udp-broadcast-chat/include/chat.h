#pragma once

#include <stdint.h>

typedef struct {
    const char *name;
    const char *broadcast_address;
    uint16_t port;
} chat_config_t;

int chat_run(const chat_config_t *config);
