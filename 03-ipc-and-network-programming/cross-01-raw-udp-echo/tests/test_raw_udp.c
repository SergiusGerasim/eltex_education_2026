#include "raw_udp.h"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IPV4_HEADER_SIZE 20U
#define SOURCE_PORT UINT16_C(40000)
#define DESTINATION_PORT UINT16_C(5000)

static int parse_address(const char *text, struct in_addr *address) {
    return inet_pton(AF_INET, text, address) == 1;
}

static int build_ip_udp_packet(
    uint8_t *packet,
    size_t capacity,
    size_t *packet_size,
    struct in_addr source_ip,
    struct in_addr destination_ip,
    const uint8_t *payload,
    size_t payload_size
) {
    if (packet_size == NULL) return 0;
    *packet_size = 0;

    if (packet == NULL || capacity < IPV4_HEADER_SIZE) return 0;

    size_t udp_size = 0;
    if (!raw_udp_build(
        packet + IPV4_HEADER_SIZE,
        capacity - IPV4_HEADER_SIZE,
        &udp_size,
        source_ip,
        destination_ip,
        SOURCE_PORT,
        DESTINATION_PORT,
        payload,
        payload_size
    )) {
        return 0;
    }

    const size_t total_size = IPV4_HEADER_SIZE + udp_size;
    if (total_size > UINT16_MAX) return 0;

    const struct iphdr ip_header = {
        .ihl = 5U,
        .version = 4U,
        .tot_len = htons((uint16_t)total_size),
        .protocol = IPPROTO_UDP,
        .saddr = source_ip.s_addr,
        .daddr = destination_ip.s_addr
    };

    memcpy(packet, &ip_header, sizeof(ip_header));
    *packet_size = total_size;
    return 1;
}

static int test_build_and_parse(void) {
    static const uint8_t payload[] = {'h', 'e', 'l', 'l', 'o'};
    uint8_t packet[IPV4_HEADER_SIZE + RAW_UDP_HEADER_SIZE + sizeof(payload)];
    struct in_addr source_ip;
    struct in_addr destination_ip;
    size_t packet_size = 0;

    if (!parse_address("192.0.2.1", &source_ip)) return 0;
    if (!parse_address("198.51.100.2", &destination_ip)) return 0;
    if (!build_ip_udp_packet(
        packet,
        sizeof(packet),
        &packet_size,
        source_ip,
        destination_ip,
        payload,
        sizeof(payload)
    )) {
        return 0;
    }

    raw_udp_datagram_t datagram;
    if (!raw_udp_parse(packet, packet_size, &datagram)) return 0;

    if (datagram.source_ip.s_addr != source_ip.s_addr) return 0;
    if (datagram.destination_ip.s_addr != destination_ip.s_addr) return 0;
    if (datagram.source_port != SOURCE_PORT) return 0;
    if (datagram.destination_port != DESTINATION_PORT) return 0;
    if (datagram.payload_size != sizeof(payload)) return 0;

    return memcmp(datagram.payload, payload, sizeof(payload)) == 0;
}

static int test_corrupted_payload(void) {
    static const uint8_t payload[] = {'h', 'e', 'l', 'l', 'o'};
    uint8_t packet[IPV4_HEADER_SIZE + RAW_UDP_HEADER_SIZE + sizeof(payload)];
    struct in_addr source_ip;
    struct in_addr destination_ip;
    size_t packet_size = 0;

    if (!parse_address("192.0.2.1", &source_ip)) return 0;
    if (!parse_address("198.51.100.2", &destination_ip)) return 0;
    if (!build_ip_udp_packet(
        packet,
        sizeof(packet),
        &packet_size,
        source_ip,
        destination_ip,
        payload,
        sizeof(payload)
    )) {
        return 0;
    }

    packet[packet_size - 1U] ^= UINT8_C(1);

    raw_udp_datagram_t datagram;
    return !raw_udp_parse(packet, packet_size, &datagram);
}

static int test_invalid_build_arguments(void) {
    uint8_t packet[RAW_UDP_HEADER_SIZE];
    struct in_addr address = {.s_addr = htonl(INADDR_LOOPBACK)};
    size_t packet_size = 99U;

    if (raw_udp_build(
        NULL,
        sizeof(packet),
        &packet_size,
        address,
        address,
        SOURCE_PORT,
        DESTINATION_PORT,
        NULL,
        0U
    )) {
        return 0;
    }
    if (packet_size != 0U) return 0;

    packet_size = 99U;
    if (raw_udp_build(
        packet,
        sizeof(packet),
        &packet_size,
        address,
        address,
        SOURCE_PORT,
        DESTINATION_PORT,
        NULL,
        1U
    )) {
        return 0;
    }
    if (packet_size != 0U) return 0;

    return !raw_udp_build(
        packet,
        RAW_UDP_HEADER_SIZE - 1U,
        &packet_size,
        address,
        address,
        SOURCE_PORT,
        DESTINATION_PORT,
        NULL,
        0U
    );
}

static int test_truncated_packet(void) {
    uint8_t packet[IPV4_HEADER_SIZE] = {0};
    raw_udp_datagram_t datagram;

    return !raw_udp_parse(packet, sizeof(packet) - 1U, &datagram);
}

int main(void) {
    if (!test_build_and_parse()) {
        fprintf(stderr, "UDP build/parse test failed\n");
        return 1;
    }
    if (!test_corrupted_payload()) {
        fprintf(stderr, "UDP checksum test failed\n");
        return 1;
    }
    if (!test_invalid_build_arguments()) {
        fprintf(stderr, "UDP invalid build arguments test failed\n");
        return 1;
    }
    if (!test_truncated_packet()) {
        fprintf(stderr, "UDP truncated packet test failed\n");
        return 1;
    }

    puts("Raw UDP tests passed");
    return 0;
}
