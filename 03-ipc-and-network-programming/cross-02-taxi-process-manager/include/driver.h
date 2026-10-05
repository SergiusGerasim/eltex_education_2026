#pragma once

#include <stdint.h>

typedef struct {
    const char *server_address;
    uint16_t server_port;
} driver_config_t;

int driver_run(const driver_config_t *config);
