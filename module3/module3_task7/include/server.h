#pragma once

#include <stdint.h>

typedef struct {
    const char *listen_address;
    uint16_t port;
} server_config_t;

int server_run(const server_config_t *config);
