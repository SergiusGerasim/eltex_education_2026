#include "protocol.h"

#include <arpa/inet.h>
#include <string.h>

#define MAGIC_OFFSET 0U
#define VERSION_OFFSET 4U
#define TYPE_OFFSET 5U
#define SENDER_ID_OFFSET 6U
#define NAME_LENGTH_OFFSET 14U
#define TEXT_LENGTH_OFFSET 16U

static bool packet_type_is_valid(chat_packet_type_t type) {
    return type == CHAT_PACKET_JOIN || type == CHAT_PACKET_MESSAGE || type == CHAT_PACKET_LEAVE;
}

static void write_u16(uint8_t *destination, uint16_t value) {
    const uint16_t network_value = htons(value);
    memcpy(destination, &network_value, sizeof(network_value));
}

static void write_u32(uint8_t *destination, uint32_t value) {
    const uint32_t network_value = htonl(value);
    memcpy(destination, &network_value, sizeof(network_value));
}

static void write_u64(uint8_t *destination, uint64_t value) {
    write_u32(destination, (uint32_t)(value >> 32U));
    write_u32(destination + sizeof(uint32_t), (uint32_t)value);
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

static uint64_t read_u64(const uint8_t *source) {
    return ((uint64_t)read_u32(source) << 32U) | read_u32(source + sizeof(uint32_t));
}

bool protocol_encode(const chat_message_t *message, uint8_t *buffer, size_t capacity, size_t *encoded_size) {
    if (encoded_size == NULL) return false;
    *encoded_size = 0;
    if (message == NULL || buffer == NULL) return false;
    if (!packet_type_is_valid(message->type) || message->sender_id == 0) return false;

    const size_t name_size = strnlen(message->sender_name, sizeof(message->sender_name));
    const size_t text_size = strnlen(message->text, sizeof(message->text));

    if (name_size == 0 || name_size > CHAT_NAME_MAX) return false;
    if (text_size > CHAT_TEXT_MAX) return false;
    if (message->type == CHAT_PACKET_MESSAGE && text_size == 0) return false;
    if (message->type != CHAT_PACKET_MESSAGE && text_size != 0) return false;

    const size_t packet_size = CHAT_PACKET_HEADER_SIZE + name_size + text_size;
    if (capacity < packet_size) return false;

    write_u32(buffer + MAGIC_OFFSET, CHAT_PROTOCOL_MAGIC);
    buffer[VERSION_OFFSET] = CHAT_PROTOCOL_VERSION;
    buffer[TYPE_OFFSET] = (uint8_t)message->type;
    write_u64(buffer + SENDER_ID_OFFSET, message->sender_id);
    write_u16(buffer + NAME_LENGTH_OFFSET, (uint16_t)name_size);
    write_u16(buffer + TEXT_LENGTH_OFFSET, (uint16_t)text_size);
    memcpy(buffer + CHAT_PACKET_HEADER_SIZE, message->sender_name, name_size);
    memcpy(buffer + CHAT_PACKET_HEADER_SIZE + name_size, message->text, text_size);

    *encoded_size = packet_size;

    return true;
}

bool protocol_decode(const uint8_t *buffer, size_t size, chat_message_t *message) {
    if (buffer == NULL || message == NULL || size < CHAT_PACKET_HEADER_SIZE) return false;
    if (read_u32(buffer + MAGIC_OFFSET) != CHAT_PROTOCOL_MAGIC) return false;
    if (buffer[VERSION_OFFSET] != CHAT_PROTOCOL_VERSION) return false;

    const chat_packet_type_t type = (chat_packet_type_t)buffer[TYPE_OFFSET];
    const uint64_t sender_id = read_u64(buffer + SENDER_ID_OFFSET);
    const size_t name_size = read_u16(buffer + NAME_LENGTH_OFFSET);
    const size_t text_size = read_u16(buffer + TEXT_LENGTH_OFFSET);

    if (!packet_type_is_valid(type) || sender_id == 0) return false;
    if (name_size == 0 || name_size > CHAT_NAME_MAX || text_size > CHAT_TEXT_MAX) return false;
    if (size != CHAT_PACKET_HEADER_SIZE + name_size + text_size) return false;
    if (type == CHAT_PACKET_MESSAGE && text_size == 0) return false;
    if (type != CHAT_PACKET_MESSAGE && text_size != 0) return false;

    message->type = type;
    message->sender_id = sender_id;
    memcpy(message->sender_name, buffer + CHAT_PACKET_HEADER_SIZE, name_size);
    message->sender_name[name_size] = '\0';
    memcpy(message->text, buffer + CHAT_PACKET_HEADER_SIZE + name_size, text_size);
    message->text[text_size] = '\0';

    return true;
}
