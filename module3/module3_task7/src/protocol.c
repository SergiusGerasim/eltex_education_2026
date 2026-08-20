#include "protocol.h"

#include <arpa/inet.h>
#include <string.h>

#define MAGIC_OFFSET 0U
#define VERSION_OFFSET 4U
#define TYPE_OFFSET 5U
#define FLAGS_OFFSET 6U
#define BODY_SIZE_OFFSET 8U
#define SENDER_ID_OFFSET 12U
#define TRANSFER_ID_OFFSET 20U
#define NAME_SIZE_OFFSET 28U
#define CONTENT_SIZE_OFFSET 30U
#define FILE_SIZE_FIELD_SIZE 8U

static bool packet_type_is_valid(chat_packet_type_t type) {
    return type >= CHAT_PACKET_JOIN && type <= CHAT_PACKET_FILE_END;
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

static bool lengths_are_valid(chat_packet_type_t type, uint64_t transfer_id, size_t content_size) {
    if (type == CHAT_PACKET_JOIN || type == CHAT_PACKET_LEAVE) return transfer_id == 0 && content_size == 0;
    if (type == CHAT_PACKET_MESSAGE) return transfer_id == 0 && content_size >= 1U && content_size <= CHAT_TEXT_MAX;
    if (type == CHAT_PACKET_FILE_BEGIN) return transfer_id != 0 && content_size >= FILE_SIZE_FIELD_SIZE + 1U && content_size <= FILE_SIZE_FIELD_SIZE + CHAT_FILE_NAME_MAX;
    if (type == CHAT_PACKET_FILE_CHUNK) return transfer_id != 0 && content_size >= 1U && content_size <= CHAT_FILE_CHUNK_MAX;
    if (type == CHAT_PACKET_FILE_END) return transfer_id != 0 && content_size == 0;
    return false;
}

static bool header_is_valid(const uint8_t *buffer, size_t *body_size) {
    if (read_u32(buffer + MAGIC_OFFSET) != CHAT_PROTOCOL_MAGIC) return false;
    if (buffer[VERSION_OFFSET] != CHAT_PROTOCOL_VERSION) return false;
    if (read_u16(buffer + FLAGS_OFFSET) != 0) return false;

    const chat_packet_type_t type = (chat_packet_type_t)buffer[TYPE_OFFSET];
    const size_t declared_body_size = read_u32(buffer + BODY_SIZE_OFFSET);
    const uint64_t sender_id = read_u64(buffer + SENDER_ID_OFFSET);
    const uint64_t transfer_id = read_u64(buffer + TRANSFER_ID_OFFSET);
    const size_t name_size = read_u16(buffer + NAME_SIZE_OFFSET);
    const size_t content_size = read_u16(buffer + CONTENT_SIZE_OFFSET);

    if (!packet_type_is_valid(type) || sender_id == 0) return false;
    if (name_size == 0 || name_size > CHAT_NAME_MAX) return false;
    if (declared_body_size != name_size + content_size) return false;
    if (declared_body_size > CHAT_NAME_MAX + CHAT_FILE_CHUNK_MAX) return false;
    if (!lengths_are_valid(type, transfer_id, content_size)) return false;

    *body_size = declared_body_size;
    return true;
}

protocol_frame_status_t protocol_frame_size(const uint8_t *buffer, size_t available_size, size_t *frame_size) {
    if (frame_size == NULL) return PROTOCOL_FRAME_INVALID;
    *frame_size = 0;
    if (buffer == NULL && available_size != 0) return PROTOCOL_FRAME_INVALID;
    if (available_size < CHAT_PACKET_HEADER_SIZE) return PROTOCOL_FRAME_INCOMPLETE;

    size_t body_size;
    if (!header_is_valid(buffer, &body_size)) return PROTOCOL_FRAME_INVALID;

    *frame_size = CHAT_PACKET_HEADER_SIZE + body_size;
    if (available_size < *frame_size) return PROTOCOL_FRAME_INCOMPLETE;
    return PROTOCOL_FRAME_COMPLETE;
}

static bool message_content(const chat_message_t *message, const uint8_t **content, size_t *content_size, size_t *file_name_size) {
    *content = NULL;
    *content_size = 0;
    *file_name_size = 0;

    if (message->type == CHAT_PACKET_JOIN || message->type == CHAT_PACKET_LEAVE || message->type == CHAT_PACKET_FILE_END) return true;
    if (message->type == CHAT_PACKET_MESSAGE) {
        *content_size = strnlen(message->text, sizeof(message->text));
        *content = (const uint8_t *)message->text;
        return *content_size <= CHAT_TEXT_MAX;
    }
    if (message->type == CHAT_PACKET_FILE_BEGIN) {
        *file_name_size = strnlen(message->file_name, sizeof(message->file_name));
        *content_size = FILE_SIZE_FIELD_SIZE + *file_name_size;
        return *file_name_size >= 1U && *file_name_size <= CHAT_FILE_NAME_MAX;
    }
    if (message->type == CHAT_PACKET_FILE_CHUNK) {
        *content = message->file_data;
        *content_size = message->file_data_size;
        return *content_size <= CHAT_FILE_CHUNK_MAX;
    }
    return false;
}

bool protocol_encode(const chat_message_t *message, uint8_t *buffer, size_t capacity, size_t *encoded_size) {
    if (encoded_size == NULL) return false;
    *encoded_size = 0;
    if (message == NULL || buffer == NULL || !packet_type_is_valid(message->type) || message->sender_id == 0) return false;

    const size_t name_size = strnlen(message->sender_name, sizeof(message->sender_name));
    const uint8_t *content;
    size_t content_size;
    size_t file_name_size;

    if (name_size == 0 || name_size > CHAT_NAME_MAX) return false;
    if (!message_content(message, &content, &content_size, &file_name_size)) return false;
    if (!lengths_are_valid(message->type, message->transfer_id, content_size)) return false;

    const size_t body_size = name_size + content_size;
    const size_t packet_size = CHAT_PACKET_HEADER_SIZE + body_size;
    if (capacity < packet_size) return false;

    write_u32(buffer + MAGIC_OFFSET, CHAT_PROTOCOL_MAGIC);
    buffer[VERSION_OFFSET] = CHAT_PROTOCOL_VERSION;
    buffer[TYPE_OFFSET] = (uint8_t)message->type;
    write_u16(buffer + FLAGS_OFFSET, 0);
    write_u32(buffer + BODY_SIZE_OFFSET, (uint32_t)body_size);
    write_u64(buffer + SENDER_ID_OFFSET, message->sender_id);
    write_u64(buffer + TRANSFER_ID_OFFSET, message->transfer_id);
    write_u16(buffer + NAME_SIZE_OFFSET, (uint16_t)name_size);
    write_u16(buffer + CONTENT_SIZE_OFFSET, (uint16_t)content_size);
    memcpy(buffer + CHAT_PACKET_HEADER_SIZE, message->sender_name, name_size);

    uint8_t *encoded_content = buffer + CHAT_PACKET_HEADER_SIZE + name_size;
    if (message->type == CHAT_PACKET_FILE_BEGIN) {
        write_u64(encoded_content, message->file_size);
        memcpy(encoded_content + FILE_SIZE_FIELD_SIZE, message->file_name, file_name_size);
    } else if (content_size != 0) {
        memcpy(encoded_content, content, content_size);
    }

    *encoded_size = packet_size;
    return true;
}

bool protocol_decode(const uint8_t *buffer, size_t size, chat_message_t *message) {
    if (message == NULL) return false;

    size_t frame_size;
    if (protocol_frame_size(buffer, size, &frame_size) != PROTOCOL_FRAME_COMPLETE || frame_size != size) return false;

    const chat_packet_type_t type = (chat_packet_type_t)buffer[TYPE_OFFSET];
    const size_t name_size = read_u16(buffer + NAME_SIZE_OFFSET);
    const size_t content_size = read_u16(buffer + CONTENT_SIZE_OFFSET);
    const uint8_t *name = buffer + CHAT_PACKET_HEADER_SIZE;
    const uint8_t *content = name + name_size;

    if (memchr(name, '\0', name_size) != NULL) return false;
    if (type == CHAT_PACKET_MESSAGE && memchr(content, '\0', content_size) != NULL) return false;
    if (type == CHAT_PACKET_FILE_BEGIN && memchr(content + FILE_SIZE_FIELD_SIZE, '\0', content_size - FILE_SIZE_FIELD_SIZE) != NULL) return false;

    memset(message, 0, sizeof(*message));
    message->type = type;
    message->sender_id = read_u64(buffer + SENDER_ID_OFFSET);
    message->transfer_id = read_u64(buffer + TRANSFER_ID_OFFSET);
    memcpy(message->sender_name, name, name_size);

    if (type == CHAT_PACKET_MESSAGE) {
        memcpy(message->text, content, content_size);
    } else if (type == CHAT_PACKET_FILE_BEGIN) {
        const size_t file_name_size = content_size - FILE_SIZE_FIELD_SIZE;
        message->file_size = read_u64(content);
        memcpy(message->file_name, content + FILE_SIZE_FIELD_SIZE, file_name_size);
    } else if (type == CHAT_PACKET_FILE_CHUNK) {
        memcpy(message->file_data, content, content_size);
        message->file_data_size = content_size;
    }

    return true;
}
