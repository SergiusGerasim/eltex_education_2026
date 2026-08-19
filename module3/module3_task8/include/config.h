#pragma once

#include <stdint.h>

#define CAPTURE_OUTPUT_DIRECTORY "captures"
#define CAPTURE_OUTPUT_NAME_MAX 255U
#define CAPTURE_OUTPUT_PATH_SIZE (sizeof(CAPTURE_OUTPUT_DIRECTORY) + CAPTURE_OUTPUT_NAME_MAX + 1U)

typedef enum {
    CAPTURE_FILTER_CHAT,
    CAPTURE_FILTER_DNS
} capture_filter_t;

typedef struct {
    const char *interface_name;
    capture_filter_t filter;
    char output_path[CAPTURE_OUTPUT_PATH_SIZE];
    uint32_t duration_seconds;
} capture_config_t;

int config_parse(int argc, char **argv, capture_config_t *config);
const char *config_filter_name(capture_filter_t filter);
