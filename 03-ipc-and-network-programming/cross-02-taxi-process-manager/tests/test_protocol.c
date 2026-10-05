#include "protocol.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_round_trip(void) {
    const taxi_message_t source = {
        .type = TAXI_MESSAGE_SEND_TASK,
        .request_id = UINT32_C(42),
        .driver_pid = UINT32_C(12345),
        .task_timer = UINT32_C(60)
    };
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t encoded_size = 0;

    assert(protocol_encode(&source, buffer, sizeof(buffer), &encoded_size));
    assert(encoded_size == TAXI_PROTOCOL_FRAME_SIZE);

    taxi_message_t decoded = {0};
    assert(protocol_decode(buffer, encoded_size, &decoded));
    assert(decoded.type == source.type);
    assert(decoded.request_id == source.request_id);
    assert(decoded.driver_pid == source.driver_pid);
    assert(decoded.task_timer == source.task_timer);
}

static void test_partial_frame(void) {
    const taxi_message_t message = {.type = TAXI_MESSAGE_GET_STATUS};
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t encoded_size = 0;
    size_t frame_size = 0;

    assert(protocol_encode(&message, buffer, sizeof(buffer), &encoded_size));
    assert(protocol_frame_size(buffer, TAXI_PROTOCOL_HEADER_SIZE - 1U, &frame_size) == PROTOCOL_FRAME_INCOMPLETE);
    assert(protocol_frame_size(buffer, TAXI_PROTOCOL_HEADER_SIZE, &frame_size) == PROTOCOL_FRAME_INCOMPLETE);
    assert(frame_size == TAXI_PROTOCOL_FRAME_SIZE);
    assert(protocol_frame_size(buffer, encoded_size, &frame_size) == PROTOCOL_FRAME_COMPLETE);
}

static void test_invalid_header(void) {
    const taxi_message_t message = {.type = TAXI_MESSAGE_HELLO};
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t encoded_size = 0;
    size_t frame_size = 0;

    assert(protocol_encode(&message, buffer, sizeof(buffer), &encoded_size));

    buffer[0] ^= UINT8_C(1);
    assert(protocol_frame_size(buffer, encoded_size, &frame_size) == PROTOCOL_FRAME_INVALID);
    buffer[0] ^= UINT8_C(1);

    buffer[4] = UINT8_C(2);
    assert(protocol_frame_size(buffer, encoded_size, &frame_size) == PROTOCOL_FRAME_INVALID);
    buffer[4] = TAXI_PROTOCOL_VERSION;

    buffer[5] = UINT8_C(255);
    assert(protocol_frame_size(buffer, encoded_size, &frame_size) == PROTOCOL_FRAME_INVALID);
}

static void test_invalid_arguments(void) {
    const taxi_message_t valid_message = {.type = TAXI_MESSAGE_SHUTDOWN};
    const taxi_message_t invalid_message = {.type = (taxi_message_type_t)0};
    const taxi_message_t wrapped_message = {.type = (taxi_message_type_t)257};
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t encoded_size = 0;
    size_t frame_size = 0;
    taxi_message_t decoded = {0};

    assert(!protocol_encode(NULL, buffer, sizeof(buffer), &encoded_size));
    assert(!protocol_encode(&valid_message, NULL, sizeof(buffer), &encoded_size));
    assert(!protocol_encode(&valid_message, buffer, sizeof(buffer) - 1U, &encoded_size));
    assert(!protocol_encode(&invalid_message, buffer, sizeof(buffer), &encoded_size));
    assert(!protocol_encode(&wrapped_message, buffer, sizeof(buffer), &encoded_size));
    assert(protocol_frame_size(NULL, sizeof(buffer), &frame_size) == PROTOCOL_FRAME_INVALID);
    assert(protocol_frame_size(buffer, sizeof(buffer), NULL) == PROTOCOL_FRAME_INVALID);

    assert(protocol_encode(&valid_message, buffer, sizeof(buffer), &encoded_size));
    assert(!protocol_decode(buffer, encoded_size - 1U, &decoded));
    assert(!protocol_decode(buffer, encoded_size, NULL));
}

static void test_network_byte_order(void) {
    const taxi_message_t message = {
        .type = TAXI_MESSAGE_STATUS_BUSY,
        .request_id = UINT32_C(0x01020304),
        .driver_pid = UINT32_C(0x11223344),
        .task_timer = UINT32_C(0xA1B2C3D4)
    };
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t encoded_size = 0;

    assert(protocol_encode(&message, buffer, sizeof(buffer), &encoded_size));
    assert(buffer[12] == UINT8_C(0x01));
    assert(buffer[13] == UINT8_C(0x02));
    assert(buffer[14] == UINT8_C(0x03));
    assert(buffer[15] == UINT8_C(0x04));
    assert(buffer[16] == UINT8_C(0x11));
    assert(buffer[20] == UINT8_C(0xA1));
}

int main(void) {
    test_round_trip();
    test_partial_frame();
    test_invalid_header();
    test_invalid_arguments();
    test_network_byte_order();
    puts("protocol tests passed");
    return 0;
}
