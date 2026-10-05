#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAXI_PROTOCOL_MAGIC UINT32_C(0x54415849)
#define TAXI_PROTOCOL_VERSION UINT8_C(1)
#define TAXI_PROTOCOL_HEADER_SIZE 16U
#define TAXI_PROTOCOL_PAYLOAD_SIZE 8U
#define TAXI_PROTOCOL_FRAME_SIZE (TAXI_PROTOCOL_HEADER_SIZE + TAXI_PROTOCOL_PAYLOAD_SIZE)

typedef enum {
    TAXI_MESSAGE_HELLO = 1,
    TAXI_MESSAGE_SEND_TASK = 2,
    TAXI_MESSAGE_GET_STATUS = 3,
    TAXI_MESSAGE_SHUTDOWN = 4,
    TAXI_MESSAGE_TASK_ACCEPTED = 5,
    TAXI_MESSAGE_STATUS_AVAILABLE = 6,
    TAXI_MESSAGE_STATUS_BUSY = 7,
    TAXI_MESSAGE_ERROR = 8
} taxi_message_type_t;

typedef enum {
    TAXI_ERROR_INVALID_MESSAGE = 1,
    TAXI_ERROR_INVALID_TIMER = 2
} taxi_error_t;

typedef enum {
    PROTOCOL_FRAME_INVALID = -1,
    PROTOCOL_FRAME_INCOMPLETE = 0,
    PROTOCOL_FRAME_COMPLETE = 1
} protocol_frame_status_t;

typedef struct {
    taxi_message_type_t type;
    uint32_t request_id;
    uint32_t driver_pid;
    uint32_t task_timer;
} taxi_message_t;

protocol_frame_status_t protocol_frame_size(const uint8_t *buffer, size_t available_size, size_t *frame_size);
bool protocol_encode(const taxi_message_t *message, uint8_t *buffer, size_t capacity, size_t *encoded_size);
bool protocol_decode(const uint8_t *buffer, size_t size, taxi_message_t *message);
