#include "protocol.h"

#include <arpa/inet.h>
#include <string.h>

#define MAGIC_OFFSET 0U
#define VERSION_OFFSET 4U
#define TYPE_OFFSET 5U
#define PAYLOAD_LENGTH_OFFSET 6U
#define COUNTER_OFFSET 8U

static bool packet_type_is_valid(packet_type_t type) {
    if (type == ECHO_CLOSE || type == ECHO_REQUEST || type == ECHO_RESPONSE) return true;
    return false;
}

static bool message_fields_are_valid(packet_type_t type, uint32_t counter, size_t payload_size) {
    if (!packet_type_is_valid(type) || payload_size > PROTOCOL_PAYLOAD_MAX) return false;
    if (type == ECHO_CLOSE) return counter == 0 && payload_size == 0;
    if (type == ECHO_REQUEST) return counter == 0;
    return counter > 0;
}

static void write_u16(uint8_t *destination, uint16_t value) {
    const uint16_t network_value = htons(value);
    memcpy(destination, &network_value, sizeof(network_value));
}

static void write_u32(uint8_t *destination, uint32_t value) {
    const uint32_t network_value = htonl(value);
    memcpy(destination, &network_value, sizeof(network_value));
}

static uint16_t read_u16(const uint8_t *source) {
    uint16_t value;
    memcpy(&value, source, sizeof(value));
    return ntohs(value);
}

static uint32_t read_u32(const uint8_t *source) {
    uint32_t value;
    memcpy(&value, source, sizeof(value));
    return ntohl(value);
}

bool protocol_encode(const protocol_message_t *message, uint8_t *buffer, size_t capacity, size_t *encoded_size) {
    if (encoded_size == NULL) return false;
    *encoded_size = 0;
    if (message == NULL || buffer == NULL) return false;
    if (!message_fields_are_valid(message->type, message->counter, message->payload_size)) return false;

    const size_t packet_size = PROTOCOL_HEADER_SIZE + message->payload_size;

    if (capacity < packet_size) return false;
    write_u32(buffer + MAGIC_OFFSET, PROTOCOL_MAGIC);
    buffer[VERSION_OFFSET] = PROTOCOL_VERSION;
    buffer[TYPE_OFFSET] = (uint8_t)message->type;
    write_u16(buffer + PAYLOAD_LENGTH_OFFSET, (uint16_t)message->payload_size);
    write_u32(buffer + COUNTER_OFFSET, message->counter);
    if (message->payload_size > 0) memcpy(buffer + PROTOCOL_HEADER_SIZE, message->payload, message->payload_size);

    *encoded_size = packet_size;
    return true;
}

bool protocol_decode(const uint8_t *buffer, size_t size, protocol_message_t *message) {
    if (buffer == NULL || message == NULL || size < PROTOCOL_HEADER_SIZE) return false;
    if (read_u32(buffer + MAGIC_OFFSET) != PROTOCOL_MAGIC) return false;
    if (buffer[VERSION_OFFSET] != PROTOCOL_VERSION) return false;

    const packet_type_t type = (packet_type_t)buffer[TYPE_OFFSET];
    const size_t payload_size = read_u16(buffer + PAYLOAD_LENGTH_OFFSET);
    const uint32_t counter = read_u32(buffer + COUNTER_OFFSET);

    if (!message_fields_are_valid(type, counter, payload_size)) return false;
    if (size != PROTOCOL_HEADER_SIZE + payload_size) return false;

    message->type = type;
    message->counter = counter;
    message->payload_size = payload_size;
    if (payload_size > 0) memcpy(message->payload, buffer + PROTOCOL_HEADER_SIZE, payload_size);
    message->payload[payload_size] = '\0';

    return true;
}
