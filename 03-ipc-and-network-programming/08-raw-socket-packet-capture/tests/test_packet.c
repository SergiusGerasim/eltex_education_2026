#include "packet.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#define ETHERNET_HEADER_SIZE 14U
#define IPV4_HEADER_SIZE 20U
#define UDP_HEADER_SIZE 8U
#define PAYLOAD_SIZE 4U
#define FRAME_SIZE (ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE + UDP_HEADER_SIZE + PAYLOAD_SIZE)

static const uint8_t valid_frame[FRAME_SIZE] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
    0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
    0x08, 0x00,
    0x45, 0x00, 0x00, 0x20,
    0x12, 0x34, 0x00, 0x00,
    0x40, 0x11, 0x00, 0x00,
    192, 168, 1, 10,
    192, 168, 1, 255,
    0x13, 0x88, 0x00, 0x35,
    0x00, 0x0C, 0x00, 0x00,
    't', 'e', 's', 't'
};

static int test_valid_udp(void) {
    udp_packet_t packet;

    if (packet_parse_udp(valid_frame, sizeof(valid_frame), &packet) != PACKET_PARSE_OK) return 0;
    if (packet.source_port != 5000 || packet.destination_port != 53) return 0;
    if (packet.payload_size != PAYLOAD_SIZE || memcmp(packet.payload, "test", PAYLOAD_SIZE) != 0) return 0;
    if (memcmp(packet.destination_mac, valid_frame, ETHERNET_ADDRESS_SIZE) != 0) return 0;
    if (memcmp(packet.source_mac, valid_frame + ETHERNET_ADDRESS_SIZE, ETHERNET_ADDRESS_SIZE) != 0) return 0;

    char source_ip[INET_ADDRSTRLEN];
    char destination_ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &packet.source_ip, source_ip, sizeof(source_ip)) == NULL) return 0;
    if (inet_ntop(AF_INET, &packet.destination_ip, destination_ip, sizeof(destination_ip)) == NULL) return 0;
    return strcmp(source_ip, "192.168.1.10") == 0 && strcmp(destination_ip, "192.168.1.255") == 0;
}

static int test_unsupported_packets(void) {
    uint8_t frame[FRAME_SIZE];
    udp_packet_t packet;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[12] = 0x08;
    frame[13] = 0x06;
    if (packet_parse_udp(frame, sizeof(frame), &packet) != PACKET_PARSE_UNSUPPORTED) return 0;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE + 9U] = 6;
    return packet_parse_udp(frame, sizeof(frame), &packet) == PACKET_PARSE_UNSUPPORTED;
}

static int test_fragmented_packets(void) {
    uint8_t frame[FRAME_SIZE];
    udp_packet_t packet;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE + 6U] = 0x20;
    if (packet_parse_udp(frame, sizeof(frame), &packet) != PACKET_PARSE_FRAGMENTED) return 0;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE + 7U] = 0x01;
    return packet_parse_udp(frame, sizeof(frame), &packet) == PACKET_PARSE_FRAGMENTED;
}

static int test_malformed_packets(void) {
    uint8_t frame[FRAME_SIZE];
    udp_packet_t packet;

    if (packet_parse_udp(NULL, sizeof(valid_frame), &packet) != PACKET_PARSE_MALFORMED) return 0;
    if (packet_parse_udp(valid_frame, sizeof(valid_frame), NULL) != PACKET_PARSE_MALFORMED) return 0;
    if (packet_parse_udp(valid_frame, ETHERNET_HEADER_SIZE - 1U, &packet) != PACKET_PARSE_MALFORMED) return 0;
    if (packet_parse_udp(valid_frame, sizeof(valid_frame) - 1U, &packet) != PACKET_PARSE_MALFORMED) return 0;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE] = 0x44;
    if (packet_parse_udp(frame, sizeof(frame), &packet) != PACKET_PARSE_MALFORMED) return 0;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE] = 0x65;
    if (packet_parse_udp(frame, sizeof(frame), &packet) != PACKET_PARSE_MALFORMED) return 0;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE + 2U] = 0x01;
    frame[ETHERNET_HEADER_SIZE + 3U] = 0x00;
    if (packet_parse_udp(frame, sizeof(frame), &packet) != PACKET_PARSE_MALFORMED) return 0;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE + 4U] = 0;
    frame[ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE + 5U] = UDP_HEADER_SIZE - 1U;
    if (packet_parse_udp(frame, sizeof(frame), &packet) != PACKET_PARSE_MALFORMED) return 0;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE + 5U] = 0x40;
    return packet_parse_udp(frame, sizeof(frame), &packet) == PACKET_PARSE_MALFORMED;
}

static int test_empty_udp_payload(void) {
    uint8_t frame[ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE + UDP_HEADER_SIZE];
    udp_packet_t packet;

    memcpy(frame, valid_frame, sizeof(frame));
    frame[ETHERNET_HEADER_SIZE + 2U] = 0;
    frame[ETHERNET_HEADER_SIZE + 3U] = IPV4_HEADER_SIZE + UDP_HEADER_SIZE;
    frame[ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE + 4U] = 0;
    frame[ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE + 5U] = UDP_HEADER_SIZE;

    if (packet_parse_udp(frame, sizeof(frame), &packet) != PACKET_PARSE_OK) return 0;
    return packet.payload_size == 0;
}

int main(void) {
    if (!test_valid_udp()) {
        fprintf(stderr, "Valid UDP packet test failed\n");
        return 1;
    }
    if (!test_unsupported_packets()) {
        fprintf(stderr, "Unsupported packet test failed\n");
        return 1;
    }
    if (!test_fragmented_packets()) {
        fprintf(stderr, "Fragmented packet test failed\n");
        return 1;
    }
    if (!test_malformed_packets()) {
        fprintf(stderr, "Malformed packet test failed\n");
        return 1;
    }
    if (!test_empty_udp_payload()) {
        fprintf(stderr, "Empty UDP payload test failed\n");
        return 1;
    }

    puts("Packet parser tests passed");
    return 0;
}
