#include "filter.h"

#include <stdio.h>
#include <string.h>

#define CHAT_HEADER_SIZE 18U

static const uint8_t chat_message[] = {
    0x4D, 0x33, 0x54, 0x36,
    0x01,
    0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B,
    0x00, 0x05,
    0x00, 0x05,
    'A', 'l', 'i', 'c', 'e',
    'H', 'e', 'l', 'l', 'o'
};

static int test_chat_filter_accepts_protocol(void) {
    const udp_packet_t packet = {
        .source_port = 6000,
        .destination_port = 7000,
        .payload = chat_message,
        .payload_size = sizeof(chat_message)
    };

    if (!filter_chat_matches(&packet)) return 0;
    return filter_matches(CAPTURE_FILTER_CHAT, &packet);
}

static int test_chat_filter_rejects_invalid_payload(void) {
    uint8_t payload[sizeof(chat_message) + 1U];
    udp_packet_t packet = {.payload = payload, .payload_size = sizeof(chat_message)};

    memcpy(payload, chat_message, sizeof(chat_message));
    payload[0] ^= 1U;
    if (filter_chat_matches(&packet)) return 0;

    memcpy(payload, chat_message, sizeof(chat_message));
    payload[4] = 2;
    if (filter_chat_matches(&packet)) return 0;

    memcpy(payload, chat_message, sizeof(chat_message));
    payload[5] = 99;
    if (filter_chat_matches(&packet)) return 0;

    memcpy(payload, chat_message, sizeof(chat_message));
    memset(payload + 6U, 0, 8U);
    if (filter_chat_matches(&packet)) return 0;

    memcpy(payload, chat_message, sizeof(chat_message));
    payload[14] = 0;
    payload[15] = 0;
    if (filter_chat_matches(&packet)) return 0;

    memcpy(payload, chat_message, sizeof(chat_message));
    payload[16] = 0;
    payload[17] = 0;
    if (filter_chat_matches(&packet)) return 0;

    memcpy(payload, chat_message, sizeof(chat_message));
    packet.payload_size = sizeof(payload);
    if (filter_chat_matches(&packet)) return 0;

    if (filter_chat_matches(NULL)) return 0;
    packet.payload = NULL;
    return !filter_chat_matches(&packet);
}

static int test_dns_filter(void) {
    static const uint8_t dns_query[12] = {
        0x12, 0x34,
        0x01, 0x00,
        0x00, 0x01,
        0x00, 0x00,
        0x00, 0x00,
        0x00, 0x00
    };
    udp_packet_t packet = {
        .source_port = 49152,
        .destination_port = 53,
        .payload = dns_query,
        .payload_size = sizeof(dns_query)
    };

    if (!filter_dns_matches(&packet)) return 0;
    if (!filter_matches(CAPTURE_FILTER_DNS, &packet)) return 0;

    packet.source_port = 53;
    packet.destination_port = 49152;
    if (!filter_dns_matches(&packet)) return 0;

    packet.source_port = 5000;
    if (filter_dns_matches(&packet)) return 0;

    packet.destination_port = 53;
    packet.payload_size = sizeof(dns_query) - 1U;
    if (filter_dns_matches(&packet)) return 0;

    uint8_t reserved_flag_query[sizeof(dns_query)];
    memcpy(reserved_flag_query, dns_query, sizeof(dns_query));
    reserved_flag_query[3] |= 0x40;
    packet.payload = reserved_flag_query;
    packet.payload_size = sizeof(reserved_flag_query);
    if (filter_dns_matches(&packet)) return 0;

    return !filter_dns_matches(NULL);
}

int main(void) {
    if (!test_chat_filter_accepts_protocol()) {
        fprintf(stderr, "Valid chat filter test failed\n");
        return 1;
    }
    if (!test_chat_filter_rejects_invalid_payload()) {
        fprintf(stderr, "Invalid chat filter test failed\n");
        return 1;
    }
    if (!test_dns_filter()) {
        fprintf(stderr, "DNS filter test failed\n");
        return 1;
    }

    puts("Filter tests passed");
    return 0;
}
