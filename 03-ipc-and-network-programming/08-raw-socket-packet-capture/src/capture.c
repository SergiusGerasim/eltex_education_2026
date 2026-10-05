#include "capture.h"

#include "filter.h"
#include "output.h"
#include "packet.h"
#include "signal_handler.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define FRAME_BUFFER_SIZE 65536U
#define POLL_INTERVAL_MS 250

static double elapsed_seconds(const struct timespec *start, const struct timespec *current) {
    const time_t seconds = current->tv_sec - start->tv_sec;
    const long nanoseconds = current->tv_nsec - start->tv_nsec;

    return (double)seconds + (double)nanoseconds / 1000000000.0;
}

static int open_capture_socket(const char *interface_name) {
    errno = 0;
    const unsigned int interface_index = if_nametoindex(interface_name);
    if (interface_index == 0) {
        fprintf(stderr, "Cannot find interface '%s': %s\n", interface_name, errno == 0 ? "unknown interface" : strerror(errno));
        return -1;
    }

    const int socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (socket_fd == -1) {
        fprintf(stderr, "Cannot open RAW socket: %s\n", strerror(errno));
        return -1;
    }

    const struct sockaddr_ll address = {
        .sll_family = AF_PACKET,
        .sll_protocol = htons(ETH_P_ALL),
        .sll_ifindex = (int)interface_index
    };
    if (bind(socket_fd, (const struct sockaddr *)&address, sizeof(address)) == -1) {
        fprintf(stderr, "Cannot bind RAW socket to '%s': %s\n", interface_name, strerror(errno));
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

static void increment_counter(uint64_t *counter) {
    if (*counter != UINT64_MAX) ++*counter;
}

static int update_elapsed(const struct timespec *start, double *total_elapsed) {
    struct timespec current;
    if (clock_gettime(CLOCK_MONOTONIC, &current) == -1) {
        fprintf(stderr, "Cannot read monotonic clock: %s\n", strerror(errno));
        return 0;
    }

    *total_elapsed = elapsed_seconds(start, &current);
    return 1;
}

static int receive_frames(int socket_fd, const capture_config_t *config, capture_output_t *output, capture_statistics_t *statistics,
                          double *total_elapsed) {
    uint8_t buffer[FRAME_BUFFER_SIZE];
    struct timespec start;

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        fprintf(stderr, "Cannot read monotonic clock: %s\n", strerror(errno));
        return 0;
    }

    *statistics = (capture_statistics_t){0};
    for (;;) {
        if (!update_elapsed(&start, total_elapsed)) return 0;
        if (signal_stop_requested()) return 1;
        if (config->duration_seconds != 0 && *total_elapsed >= (double)config->duration_seconds) return 1;

        struct pollfd descriptor = {.fd = socket_fd, .events = POLLIN};
        const int poll_status = poll(&descriptor, 1, POLL_INTERVAL_MS);
        if (poll_status == -1) {
            if (errno == EINTR) continue;
            fprintf(stderr, "Cannot wait for Ethernet frames: %s\n", strerror(errno));
            return 0;
        }
        if (poll_status == 0) continue;
        if ((descriptor.revents & POLLIN) == 0) {
            fprintf(stderr, "RAW socket reported an unexpected event\n");
            return 0;
        }

        const ssize_t received = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (received == -1) {
            if (errno == EINTR) continue;
            fprintf(stderr, "Cannot receive Ethernet frame: %s\n", strerror(errno));
            return 0;
        }
        if (received <= 0) continue;

        increment_counter(&statistics->frame_count);
        udp_packet_t packet;
        const packet_parse_result_t parse_result = packet_parse_udp(buffer, (size_t)received, &packet);
        if (parse_result == PACKET_PARSE_UNSUPPORTED) {
            increment_counter(&statistics->unsupported_count);
            continue;
        }
        if (parse_result == PACKET_PARSE_FRAGMENTED) {
            increment_counter(&statistics->fragmented_count);
            continue;
        }
        if (parse_result == PACKET_PARSE_MALFORMED) {
            increment_counter(&statistics->malformed_count);
            continue;
        }

        increment_counter(&statistics->udp_count);
        if (!filter_matches(config->filter, &packet)) continue;

        increment_counter(&statistics->matched_count);
        if (!update_elapsed(&start, total_elapsed)) return 0;
        if (!output_write_packet(output, &packet, statistics->matched_count, *total_elapsed)) {
            fprintf(stderr, "Cannot write captured packet to '%s': %s\n", config->output_path, strerror(errno));
            return 0;
        }
    }
}

int capture_run(const capture_config_t *config) {
    if (config == NULL) return 0;

    capture_output_t output = {0};
    if (!output_open(&output, config->output_path)) return 0;

    if (!signal_handlers_install()) {
        fprintf(stderr, "Cannot install signal handlers: %s\n", strerror(errno));
        output_close(&output);
        return 0;
    }

    const int socket_fd = open_capture_socket(config->interface_name);
    if (socket_fd == -1) {
        signal_handlers_restore();
        output_close(&output);
        return 0;
    }

    int status = output_write_start(&output, config);
    if (!status) fprintf(stderr, "Cannot write capture header to '%s': %s\n", config->output_path, strerror(errno));
    capture_statistics_t statistics = {0};
    double total_elapsed = 0.0;
    if (status) status = receive_frames(socket_fd, config, &output, &statistics, &total_elapsed);
    if (status && !output_write_summary(&output, &statistics, total_elapsed, signal_stop_requested())) {
        fprintf(stderr, "Cannot write capture summary to '%s': %s\n", config->output_path, strerror(errno));
        status = 0;
    }

    if (close(socket_fd) == -1) {
        fprintf(stderr, "Cannot close RAW socket: %s\n", strerror(errno));
        status = 0;
    }
    signal_handlers_restore();
    if (!output_close(&output)) {
        fprintf(stderr, "Cannot close output file '%s': %s\n", config->output_path, strerror(errno));
        status = 0;
    }

    if (status) {
        fprintf(stderr, "Captured %" PRIu64 " Ethernet frames; %" PRIu64 " packets matched; report saved to %s\n", statistics.frame_count,
                statistics.matched_count, config->output_path);
    }
    return status;
}
