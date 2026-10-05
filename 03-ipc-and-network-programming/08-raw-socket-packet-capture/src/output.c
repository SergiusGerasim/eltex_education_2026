#include "output.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int output_open(capture_output_t *output, const char *path) {
    if (output == NULL || path == NULL || *path == '\0') return 0;

    if (mkdir(CAPTURE_OUTPUT_DIRECTORY, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "Cannot create output directory '%s': %s\n", CAPTURE_OUTPUT_DIRECTORY, strerror(errno));
        return 0;
    }

    output->file = fopen(path, "w");
    if (output->file == NULL) {
        fprintf(stderr, "Cannot open output file '%s': %s\n", path, strerror(errno));
        return 0;
    }

    return 1;
}

int output_write_start(capture_output_t *output, const capture_config_t *config) {
    if (output == NULL || output->file == NULL || config == NULL) return 0;

    if (fprintf(output->file, "Capture session\nInterface: %s\nFilter: %s\n", config->interface_name, config_filter_name(config->filter)) < 0) return 0;

    if (config->duration_seconds == 0) {
        if (fprintf(output->file, "Duration: until interrupted\n\n") < 0) return 0;
    } else if (fprintf(output->file, "Duration: %" PRIu32 " seconds\n\n", config->duration_seconds) < 0) {
        return 0;
    }

    return fflush(output->file) == 0;
}

static int write_mac_address(FILE *file, const uint8_t *address) {
    return fprintf(file, "%02x:%02x:%02x:%02x:%02x:%02x", address[0], address[1], address[2], address[3], address[4], address[5]) >= 0;
}

int output_write_packet(capture_output_t *output, const udp_packet_t *packet, uint64_t sequence_number, double elapsed_seconds) {
    if (output == NULL || output->file == NULL || packet == NULL) return 0;

    char source_ip[INET_ADDRSTRLEN];
    char destination_ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &packet->source_ip, source_ip, sizeof(source_ip)) == NULL) return 0;
    if (inet_ntop(AF_INET, &packet->destination_ip, destination_ip, sizeof(destination_ip)) == NULL) return 0;

    if (fprintf(output->file, "Packet #%" PRIu64 " [%.6f s]\nSource MAC: ", sequence_number, elapsed_seconds) < 0) return 0;
    if (!write_mac_address(output->file, packet->source_mac)) return 0;
    if (fprintf(output->file, "\nDestination MAC: ") < 0) return 0;
    if (!write_mac_address(output->file, packet->destination_mac)) return 0;
    if (fprintf(output->file,
                "\nSource IP: %s\nDestination IP: %s\nSource port: %" PRIu16 "\nDestination port: %" PRIu16
                "\nUDP payload size: %zu bytes\n\n",
                source_ip, destination_ip, packet->source_port, packet->destination_port, packet->payload_size) < 0) {
        return 0;
    }

    return fflush(output->file) == 0;
}

int output_write_summary(capture_output_t *output, const capture_statistics_t *statistics, double elapsed_seconds, int interrupted) {
    if (output == NULL || output->file == NULL || statistics == NULL) return 0;

    if (fprintf(output->file,
                "Capture summary\nElapsed: %.3f seconds\nEthernet frames received: %" PRIu64 "\nUDP datagrams parsed: %" PRIu64
                "\nPackets matched by filter: %" PRIu64 "\nUnsupported frames: %" PRIu64 "\nFragmented IPv4 packets: %" PRIu64
                "\nMalformed frames: %" PRIu64 "\nStopped by: %s\n",
                elapsed_seconds, statistics->frame_count, statistics->udp_count, statistics->matched_count, statistics->unsupported_count,
                statistics->fragmented_count, statistics->malformed_count, interrupted ? "signal" : "timer") < 0) {
        return 0;
    }

    return fflush(output->file) == 0;
}

int output_close(capture_output_t *output) {
    if (output == NULL || output->file == NULL) return 1;

    const int status = fclose(output->file);
    output->file = NULL;
    return status == 0;
}
