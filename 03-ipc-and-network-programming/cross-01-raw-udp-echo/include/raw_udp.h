#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>

#define RAW_UDP_HEADER_SIZE 8U

typedef struct {
    struct in_addr source_ip;
    struct in_addr destination_ip;
    uint16_t source_port;
    uint16_t destination_port;
    const uint8_t *payload;
    size_t payload_size;
} raw_udp_datagram_t;

int raw_udp_open(void);

bool raw_udp_build(uint8_t *packet,  size_t capacity, size_t *packet_size, struct in_addr source_ip, struct in_addr destination_ip
    , uint16_t source_port, uint16_t destination_port, const uint8_t *payload, size_t payload_size);

ssize_t raw_udp_send(int descriptor, const uint8_t *packet, size_t packet_size, struct in_addr destination_ip);

bool raw_udp_parse(const uint8_t *packet, size_t packet_size, raw_udp_datagram_t *datagram);