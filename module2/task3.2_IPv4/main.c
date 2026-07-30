#include "cheking_subnet_membership.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PACKETS_OUTPUT_FILE "generated_packets.txt"

static execution_status_t parse_packet_count(const char *text,size_t *packet_count){
    if (text == NULL || packet_count == NULL || *text == '\0') return EXECUTION_INVALID_INPUT;

    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') return EXECUTION_INVALID_INPUT;
    }

    errno = 0;
    char *end = NULL;
    uintmax_t value = strtoumax(text, &end, 10);

    if (errno == ERANGE || end == text || *end != '\0' 
        || value == 0 || value > SIZE_MAX)return EXECUTION_INVALID_INPUT;

    *packet_count = (size_t)value;
    return EXECUTION_OK;
}

static execution_status_t write_packets_to_file(
    const char *file_name,
    const ipv4_address_t *packets,
    size_t packet_count,
    const ipv4_address_t *gateway,
    const ipv4_address_t *mask){
    if (file_name == NULL || packets == NULL || packet_count == 0 ||
        gateway == NULL || mask == NULL) return EXECUTION_INVALID_INPUT;

    FILE *output = fopen(file_name, "w");
    if (output == NULL) return EXECUTION_FAIL;

    execution_status_t status = EXECUTION_OK;

    for (size_t i = 0; i < packet_count; ++i) {
        char address_text[IPV4_STRING_SIZE];

        status = ipv4_to_string(&packets[i], address_text, sizeof(address_text));
        if (status != EXECUTION_OK) break;

        const char *destination = subnet_membership(&packets[i], gateway, 
            mask) ? "local subnet" : "external network";

        if (fprintf(output,"%zu: %s - %s\n",i + 1,address_text,destination) < 0) {
            status = EXECUTION_FAIL;
            break;
        }
    }

    if (fclose(output) != 0) status = EXECUTION_FAIL;

    return status;
}

static void print_usage(const char *program_name){
    fprintf(
        stderr,
        "Usage: %s <gateway IPv4> <subnet mask> <packet count>\n"
        "Example: %s 192.168.1.1 255.255.255.0 1000\n",
        program_name,
        program_name
    );
}

int main(int argc, char *argv[]){
    if (argc != 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    ipv4_address_t gateway = {0};
    ipv4_address_t mask = {0};
    size_t packet_count = 0;

    if (ipv4_parse(argv[1], &gateway) != EXECUTION_OK) {
        fprintf(stderr, "Invalid gateway IPv4 address: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (ipv4_parse(argv[2], &mask) != EXECUTION_OK ||
        !ipv4_mask_is_valid(&mask)) {
        fprintf(stderr, "Invalid subnet mask: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    if (parse_packet_count(argv[3], &packet_count) != EXECUTION_OK) {
        fprintf(stderr, "Invalid packet count: %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    srand((unsigned int)time(NULL));

    ipv4_address_t *packets = NULL;
    execution_status_t status = generate_packets(packet_count, &packets);
    if (status != EXECUTION_OK) {
        fprintf(stderr, "Failed to generate %zu packets\n", packet_count);
        return EXIT_FAILURE;
    }

    packet_statistics_t statistics = {0};
    status = process_packets(
        packets,
        packet_count,
        &gateway,
        &mask,
        &statistics
    );
    if (status != EXECUTION_OK) {
        fprintf(stderr, "Failed to process generated packets\n");
        free(packets);
        return EXIT_FAILURE;
    }

    status = write_packets_to_file(
        PACKETS_OUTPUT_FILE,
        packets,
        packet_count,
        &gateway,
        &mask
    );
    if (status != EXECUTION_OK) {
        fprintf(stderr, "Failed to write packets to %s\n", PACKETS_OUTPUT_FILE);
        free(packets);
        return EXIT_FAILURE;
    }

    size_t external_count =
        statistics.all_count - statistics.membership_count;
    double local_percent =
        100.0 * (double)statistics.membership_count /
        (double)statistics.all_count;
    double external_percent =
        100.0 * (double)external_count /
        (double)statistics.all_count;

    printf("Processed packets: %zu\n", statistics.all_count);
    printf(
        "Local subnet: %zu (%.2f%%)\n",
        statistics.membership_count,
        local_percent
    );
    printf(
        "External networks: %zu (%.2f%%)\n",
        external_count,
        external_percent
    );
    printf("Packet addresses written to: %s\n", PACKETS_OUTPUT_FILE);

    free(packets);
    return EXIT_SUCCESS;
}
