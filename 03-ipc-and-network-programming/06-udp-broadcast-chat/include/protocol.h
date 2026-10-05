#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHAT_PROTOCOL_MAGIC UINT32_C(0x4D335436)
#define CHAT_PROTOCOL_VERSION UINT8_C(1)
#define CHAT_NAME_MAX 31U
#define CHAT_TEXT_MAX 1023U
#define CHAT_PACKET_HEADER_SIZE 18U
#define CHAT_PACKET_MAX_SIZE (CHAT_PACKET_HEADER_SIZE + CHAT_NAME_MAX + CHAT_TEXT_MAX)

typedef enum {
    CHAT_PACKET_JOIN = 1,
    CHAT_PACKET_MESSAGE = 2,
    CHAT_PACKET_LEAVE = 3
} chat_packet_type_t;

typedef struct {
    chat_packet_type_t type;
    uint64_t sender_id;
    char sender_name[CHAT_NAME_MAX + 1U];
    char text[CHAT_TEXT_MAX + 1U];
} chat_message_t;

bool protocol_encode(const chat_message_t *message, uint8_t *buffer, size_t capacity, size_t *encoded_size);
bool protocol_decode(const uint8_t *buffer, size_t size, chat_message_t *message);
