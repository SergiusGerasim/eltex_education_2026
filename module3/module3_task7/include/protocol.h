#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHAT_PROTOCOL_MAGIC UINT32_C(0x4D335437)
#define CHAT_PROTOCOL_VERSION UINT8_C(2)
#define CHAT_NAME_MAX 31U
#define CHAT_TEXT_MAX 1023U
#define CHAT_FILE_NAME_MAX 255U
#define CHAT_FILE_CHUNK_MAX 16384U
#define CHAT_PACKET_HEADER_SIZE 32U
#define CHAT_PACKET_MAX_SIZE (CHAT_PACKET_HEADER_SIZE + CHAT_NAME_MAX + CHAT_FILE_CHUNK_MAX)

typedef enum {
    CHAT_PACKET_JOIN = 1,
    CHAT_PACKET_MESSAGE = 2,
    CHAT_PACKET_LEAVE = 3,
    CHAT_PACKET_FILE_BEGIN = 4,
    CHAT_PACKET_FILE_CHUNK = 5,
    CHAT_PACKET_FILE_END = 6
} chat_packet_type_t;

typedef enum {
    PROTOCOL_FRAME_INVALID = -1,
    PROTOCOL_FRAME_INCOMPLETE = 0,
    PROTOCOL_FRAME_COMPLETE = 1
} protocol_frame_status_t;

typedef struct {
    chat_packet_type_t type;
    uint64_t sender_id;
    uint64_t transfer_id;
    char sender_name[CHAT_NAME_MAX + 1U];
    char text[CHAT_TEXT_MAX + 1U];
    char file_name[CHAT_FILE_NAME_MAX + 1U];
    uint64_t file_size;
    uint8_t file_data[CHAT_FILE_CHUNK_MAX];
    size_t file_data_size;
} chat_message_t;

protocol_frame_status_t protocol_frame_size(const uint8_t *buffer, size_t available_size, size_t *frame_size);
bool protocol_encode(const chat_message_t *message, uint8_t *buffer, size_t capacity, size_t *encoded_size);
bool protocol_decode(const uint8_t *buffer, size_t size, chat_message_t *message);
