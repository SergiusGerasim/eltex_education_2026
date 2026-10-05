#include "consumer.h"
#include "producer.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s producer [segment_size] [generation_interval] [check_interval] [min_length] [max_length] [min_value] [max_value]\n",
            program);
    fprintf(stderr, "  %s consumer [read_interval]\n", program);
}

static int parse_unsigned(const char *text, unsigned long long maximum, unsigned long long *result) {
    if (text == NULL || result == NULL || *text == '\0' || *text == '-') return 0;

    errno = 0;
    char *end = NULL;
    const unsigned long long value = strtoull(text, &end, 10);

    if (errno != 0 || *end != '\0' || value > maximum) return 0;

    *result = value;
    return 1;
}

static int parse_signed_32(const char *text, int32_t *result) {
    if (text == NULL || result == NULL || *text == '\0') return 0;

    errno = 0;
    char *end = NULL;
    const long long value = strtoll(text, &end, 10);

    if (errno != 0 || *end != '\0' || value < INT32_MIN || value > INT32_MAX) return 0;

    *result = (int32_t)value;
    return 1;
}

static int run_producer(int argc, char **argv) {
    if (argc > 9) return 1;

    producer_config_t config = {
        .segment_size = 4096,
        .min_array_length = 1,
        .max_array_length = 10,
        .min_value = -100,
        .max_value = 100,
        .generation_interval_seconds = 1,
        .check_interval_seconds = 1
    };
    unsigned long long value;

    if (argc > 2 && !parse_unsigned(argv[2], SIZE_MAX, &value)) return 1;
    if (argc > 2) config.segment_size = (size_t)value;
    if (argc > 3 && !parse_unsigned(argv[3], UINT_MAX, &value)) return 1;
    if (argc > 3) config.generation_interval_seconds = (unsigned int)value;
    if (argc > 4 && !parse_unsigned(argv[4], UINT_MAX, &value)) return 1;
    if (argc > 4) config.check_interval_seconds = (unsigned int)value;
    if (argc > 5 && !parse_unsigned(argv[5], UINT32_MAX, &value)) return 1;
    if (argc > 5) config.min_array_length = (uint32_t)value;
    if (argc > 6 && !parse_unsigned(argv[6], UINT32_MAX, &value)) return 1;
    if (argc > 6) config.max_array_length = (uint32_t)value;
    if (argc > 7 && !parse_signed_32(argv[7], &config.min_value)) return 1;
    if (argc > 8 && !parse_signed_32(argv[8], &config.max_value)) return 1;

    return producer_run(&config);
}

static int run_consumer(int argc, char **argv) {
    if (argc > 3) return 1;

    consumer_config_t config = {.read_interval_seconds = 1};
    unsigned long long value;

    if (argc > 2 && !parse_unsigned(argv[2], UINT_MAX, &value)) return 1;
    if (argc > 2) config.read_interval_seconds = (unsigned int)value;

    return consumer_run(&config);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    int result;

    if (strcmp(argv[1], "producer") == 0) result = run_producer(argc, argv);
    else if (strcmp(argv[1], "consumer") == 0) result = run_consumer(argc, argv);
    else result = 1;

    if (result != 0) print_usage(argv[0]);
    return result;
}
