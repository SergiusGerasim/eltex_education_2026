#pragma once

#include "config.h"
#include "packet.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *file;
} capture_output_t;

typedef struct {
    uint64_t frame_count;
    uint64_t udp_count;
    uint64_t matched_count;
    uint64_t malformed_count;
    uint64_t fragmented_count;
    uint64_t unsupported_count;
} capture_statistics_t;

int output_open(capture_output_t *output, const char *path);
int output_write_start(capture_output_t *output, const capture_config_t *config);
int output_write_packet(capture_output_t *output, const udp_packet_t *packet, uint64_t sequence_number, double elapsed_seconds);
int output_write_summary(capture_output_t *output, const capture_statistics_t *statistics, double elapsed_seconds, int interrupted);
int output_close(capture_output_t *output);
