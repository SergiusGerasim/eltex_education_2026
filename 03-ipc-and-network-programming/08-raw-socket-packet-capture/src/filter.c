#include "filter.h"

#include <stddef.h>
#include <stdint.h>

#define CHAT_HEADER_SIZE 18U
#define CHAT_MAGIC UINT32_C(0x4D335436)
#define CHAT_VERSION UINT8_C(1)
#define CHAT_TYPE_JOIN UINT8_C(1)
#define CHAT_TYPE_MESSAGE UINT8_C(2)
#define CHAT_TYPE_LEAVE UINT8_C(3)
#define CHAT_NAME_MAX 31U
#define CHAT_TEXT_MAX 1023U
#define DNS_PORT UINT16_C(53)
#define DNS_HEADER_SIZE 12U
#define DNS_RESERVED_FLAG UINT16_C(0x0040)

static uint16_t read_u16_be(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] << 8U) | data[1];
}

static uint32_t read_u32_be(const uint8_t *data) {
    return ((uint32_t)data[0] << 24U) | ((uint32_t)data[1] << 16U) | ((uint32_t)data[2] << 8U) | data[3];
}

static int bytes_are_not_all_zero(const uint8_t *data, size_t size) {
    for (size_t index = 0; index < size; ++index) {
        if (data[index] != 0) return 1;
    }

    return 0;
}

int filter_chat_matches(const udp_packet_t *packet) {
    if (packet == NULL || packet->payload == NULL || packet->payload_size < CHAT_HEADER_SIZE) return 0;

    const uint8_t *payload = packet->payload;
    if (read_u32_be(payload) != CHAT_MAGIC || payload[4] != CHAT_VERSION) return 0;

    const uint8_t type = payload[5];
    if (type != CHAT_TYPE_JOIN && type != CHAT_TYPE_MESSAGE && type != CHAT_TYPE_LEAVE) return 0;
    if (!bytes_are_not_all_zero(payload + 6U, 8U)) return 0;

    const size_t name_size = read_u16_be(payload + 14U);
    const size_t text_size = read_u16_be(payload + 16U);
    if (name_size == 0 || name_size > CHAT_NAME_MAX || text_size > CHAT_TEXT_MAX) return 0;
    if (packet->payload_size != CHAT_HEADER_SIZE + name_size + text_size) return 0;
    if (type == CHAT_TYPE_MESSAGE && text_size == 0) return 0;
    if (type != CHAT_TYPE_MESSAGE && text_size != 0) return 0;

    return 1;
}

int filter_dns_matches(const udp_packet_t *packet) {
    if (packet == NULL || packet->payload == NULL) return 0;
    if (packet->source_port != DNS_PORT && packet->destination_port != DNS_PORT) return 0;
    if (packet->payload_size < DNS_HEADER_SIZE) return 0;

    const uint16_t flags = read_u16_be(packet->payload + 2U);
    return (flags & DNS_RESERVED_FLAG) == 0;
}

int filter_matches(capture_filter_t filter, const udp_packet_t *packet) {
    switch (filter) {
        case CAPTURE_FILTER_CHAT:
            return filter_chat_matches(packet);
        case CAPTURE_FILTER_DNS:
            return filter_dns_matches(packet);
    }

    return 0;
}
