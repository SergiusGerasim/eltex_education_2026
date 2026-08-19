#include "config.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s <interface> <chat|dns> <output_file_name> [duration_seconds]\n", program);
    fprintf(stderr, "Use duration 0 or omit it to capture until Ctrl+C\n");
    fprintf(stderr, "Reports are saved in the %s directory\n", CAPTURE_OUTPUT_DIRECTORY);
    fprintf(stderr, "Example: %s eth0 chat capture.txt 30\n", program);
}

static int parse_filter(const char *text, capture_filter_t *filter) {
    if (strcmp(text, "chat") == 0) {
        *filter = CAPTURE_FILTER_CHAT;
        return 1;
    }

    if (strcmp(text, "dns") == 0) {
        *filter = CAPTURE_FILTER_DNS;
        return 1;
    }

    return 0;
}

static int parse_duration(const char *text, uint32_t *duration_seconds) {
    if (text == NULL || *text == '\0' || *text == '-') return 0;

    errno = 0;
    char *end = NULL;
    const uintmax_t value = strtoumax(text, &end, 10);

    if (errno != 0 || *end != '\0' || value > UINT32_MAX) return 0;

    *duration_seconds = (uint32_t)value;
    return 1;
}

int config_parse(int argc, char **argv, capture_config_t *config) {
    if (config == NULL || argv == NULL || argc < 1) return 0;

    if (argc < 4 || argc > 5) {
        print_usage(argv[0]);
        return 0;
    }

    if (argv[1][0] == '\0') {
        fprintf(stderr, "Interface name must not be empty\n");
        return 0;
    }

    capture_filter_t filter;
    if (!parse_filter(argv[2], &filter)) {
        fprintf(stderr, "Unknown filter '%s'; expected chat or dns\n", argv[2]);
        return 0;
    }

    const size_t output_name_size = strnlen(argv[3], CAPTURE_OUTPUT_NAME_MAX + 1U);
    if (output_name_size == 0 || output_name_size > CAPTURE_OUTPUT_NAME_MAX || strchr(argv[3], '/') != NULL || strcmp(argv[3], ".") == 0 ||
        strcmp(argv[3], "..") == 0) {
        fprintf(stderr, "Output file name must contain from 1 to %u characters and must not contain '/'\n", CAPTURE_OUTPUT_NAME_MAX);
        return 0;
    }

    uint32_t duration_seconds = 0;
    if (argc == 5 && !parse_duration(argv[4], &duration_seconds)) {
        fprintf(stderr, "Duration must be an integer from 0 to %" PRIu32 " seconds\n", UINT32_MAX);
        return 0;
    }

    *config = (capture_config_t){
        .interface_name = argv[1],
        .filter = filter,
        .duration_seconds = duration_seconds
    };
    const int path_size = snprintf(config->output_path, sizeof(config->output_path), "%s/%s", CAPTURE_OUTPUT_DIRECTORY, argv[3]);
    if (path_size < 0 || (size_t)path_size >= sizeof(config->output_path)) return 0;

    return 1;
}

const char *config_filter_name(capture_filter_t filter) {
    switch (filter) {
        case CAPTURE_FILTER_CHAT:
            return "chat";
        case CAPTURE_FILTER_DNS:
            return "dns";
    }

    return "unknown";
}
