#pragma once 

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_MAGIC UINT32_C(0x4543484F)
#define PROTOCOL_VERSION UINT8_C(1)
#define PROTOCOL_PAYLOAD_MAX 1024U
#define PROTOCOL_HEADER_SIZE 12U
#define PROTOCOL_PACKET_MAX_SIZE (PROTOCOL_HEADER_SIZE + PROTOCOL_PAYLOAD_MAX)

typedef enum {
    ECHO_REQUEST = 1,
    ECHO_RESPONSE = 2,
    ECHO_CLOSE = 3
} packet_type_t;

typedef struct {
    packet_type_t type;
    uint32_t counter;
    size_t payload_size;
    uint8_t payload[PROTOCOL_PAYLOAD_MAX + 1U];
} protocol_message_t;

bool protocol_encode(const protocol_message_t *message, uint8_t *buffer, size_t  capacity, size_t *encoded_size);
bool protocol_decode(const uint8_t *buffer, size_t size, protocol_message_t *message);