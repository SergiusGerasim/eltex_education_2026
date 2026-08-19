#pragma once

#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

#define ETHERNET_ADDRESS_SIZE 6U

typedef struct {
    uint8_t source_mac[ETHERNET_ADDRESS_SIZE];
    uint8_t destination_mac[ETHERNET_ADDRESS_SIZE];
    struct in_addr source_ip;
    struct in_addr destination_ip;
    uint16_t source_port;
    uint16_t destination_port;
    const uint8_t *payload;
    size_t payload_size;
} udp_packet_t;

typedef enum {
    PACKET_PARSE_OK,
    PACKET_PARSE_UNSUPPORTED,
    PACKET_PARSE_FRAGMENTED,
    PACKET_PARSE_MALFORMED
} packet_parse_result_t;

packet_parse_result_t packet_parse_udp(const uint8_t *frame, size_t frame_size, udp_packet_t *packet);
