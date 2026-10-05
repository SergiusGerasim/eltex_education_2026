#include "protocol.h"

#include <arpa/inet.h>
#include <string.h>

#define MAGIC_OFFSET 0U
#define VERSION_OFFSET 4U
#define TYPE_OFFSET 5U
#define RESERVED_OFFSET 6U
#define PAYLOAD_SIZE_OFFSET 8U
#define REQUEST_ID_OFFSET 12U
#define DRIVER_PID_OFFSET 16U
#define TASK_TIMER_OFFSET 20U

static bool message_type_is_valid(taxi_message_type_t type) {
    return type >= TAXI_MESSAGE_HELLO && type <= TAXI_MESSAGE_ERROR;
}

static void write_uint32(uint8_t *buffer, size_t offset, uint32_t value) {
    const uint32_t network_value = htonl(value);
    memcpy(buffer + offset, &network_value, sizeof(network_value));
}

static uint32_t read_uint32(const uint8_t *buffer, size_t offset) {
    uint32_t network_value = 0;
    memcpy(&network_value, buffer + offset, sizeof(network_value));
    return ntohl(network_value);
}

protocol_frame_status_t protocol_frame_size(const uint8_t *buffer, size_t available_size, size_t *frame_size) {
    if (buffer == NULL || frame_size == NULL) return PROTOCOL_FRAME_INVALID;
    if (available_size < TAXI_PROTOCOL_HEADER_SIZE) return PROTOCOL_FRAME_INCOMPLETE;

    if (read_uint32(buffer, MAGIC_OFFSET) != TAXI_PROTOCOL_MAGIC ||
        buffer[VERSION_OFFSET] != TAXI_PROTOCOL_VERSION ||
        !message_type_is_valid((taxi_message_type_t)buffer[TYPE_OFFSET]) ||
        buffer[RESERVED_OFFSET] != 0 ||
        buffer[RESERVED_OFFSET + 1U] != 0 ||
        read_uint32(buffer, PAYLOAD_SIZE_OFFSET) != TAXI_PROTOCOL_PAYLOAD_SIZE) {
        return PROTOCOL_FRAME_INVALID;
    }

    *frame_size = TAXI_PROTOCOL_FRAME_SIZE;
    return available_size < TAXI_PROTOCOL_FRAME_SIZE ? PROTOCOL_FRAME_INCOMPLETE : PROTOCOL_FRAME_COMPLETE;
}

bool protocol_encode(const taxi_message_t *message, uint8_t *buffer, size_t capacity, size_t *encoded_size) {
    if (message == NULL || buffer == NULL || encoded_size == NULL ||
        capacity < TAXI_PROTOCOL_FRAME_SIZE || !message_type_is_valid(message->type)) {
        return false;
    }

    memset(buffer, 0, TAXI_PROTOCOL_FRAME_SIZE);
    write_uint32(buffer, MAGIC_OFFSET, TAXI_PROTOCOL_MAGIC);
    buffer[VERSION_OFFSET] = TAXI_PROTOCOL_VERSION;
    buffer[TYPE_OFFSET] = (uint8_t)message->type;
    write_uint32(buffer, PAYLOAD_SIZE_OFFSET, TAXI_PROTOCOL_PAYLOAD_SIZE);
    write_uint32(buffer, REQUEST_ID_OFFSET, message->request_id);
    write_uint32(buffer, DRIVER_PID_OFFSET, message->driver_pid);
    write_uint32(buffer, TASK_TIMER_OFFSET, message->task_timer);

    *encoded_size = TAXI_PROTOCOL_FRAME_SIZE;
    return true;
}

bool protocol_decode(const uint8_t *buffer, size_t size, taxi_message_t *message) {
    size_t frame_size = 0;
    if (message == NULL || protocol_frame_size(buffer, size, &frame_size) != PROTOCOL_FRAME_COMPLETE ||
        size != frame_size) {
        return false;
    }

    message->type = (taxi_message_type_t)buffer[TYPE_OFFSET];
    message->request_id = read_uint32(buffer, REQUEST_ID_OFFSET);
    message->driver_pid = read_uint32(buffer, DRIVER_PID_OFFSET);
    message->task_timer = read_uint32(buffer, TASK_TIMER_OFFSET);
    return true;
}
