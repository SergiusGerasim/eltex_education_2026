#pragma once

#include <stdint.h>

typedef struct {
    const char *name;
    const char *server_address;
    uint16_t port;
} client_config_t;

int client_run(const client_config_t *config);
