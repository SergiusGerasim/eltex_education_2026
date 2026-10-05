#include "protocol.h"

#include <stdio.h>
#include <string.h>

#define MAGIC_OFFSET 0U
#define VERSION_OFFSET 4U
#define TYPE_OFFSET 5U
#define PAYLOAD_LENGTH_OFFSET 6U
#define COUNTER_OFFSET 8U

static int messages_are_equal(const protocol_message_t *actual, const protocol_message_t *expected) {
    if (actual->type != expected->type) return 0;
    if (actual->counter != expected->counter) return 0;
    if (actual->payload_size != expected->payload_size) return 0;
    return memcmp(actual->payload, expected->payload, actual->payload_size) == 0;
}

static int test_expected_bytes(void) {
    static const uint8_t expected[] = {
        0x45, 0x43, 0x48, 0x4F,
        0x01,
        0x01,
        0x00, 0x04,
        0x00, 0x00, 0x00, 0x00,
        'p', 'i', 'n', 'g'
    };
    protocol_message_t message = {.type = ECHO_REQUEST, .payload_size = 4U};
    uint8_t buffer[PROTOCOL_PACKET_MAX_SIZE];
    size_t encoded_size = 0;

    memcpy(message.payload, "ping", message.payload_size);
    if (!protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;
    if (encoded_size != sizeof(expected)) return 0;
    return memcmp(buffer, expected, sizeof(expected)) == 0;
}

static int test_round_trip(packet_type_t type, uint32_t counter, const char *payload) {
    protocol_message_t source = {.type = type, .counter = counter};
    protocol_message_t decoded = {0};
    uint8_t buffer[PROTOCOL_PACKET_MAX_SIZE];
    size_t encoded_size = 0;

    source.payload_size = strlen(payload);
    memcpy(source.payload, payload, source.payload_size);

    if (!protocol_encode(&source, buffer, sizeof(buffer), &encoded_size)) return 0;
    if (!protocol_decode(buffer, encoded_size, &decoded)) return 0;
    if (decoded.payload[decoded.payload_size] != '\0') return 0;
    return messages_are_equal(&decoded, &source);
}

static int encode_request(uint8_t *buffer, size_t *encoded_size) {
    protocol_message_t message = {.type = ECHO_REQUEST, .payload_size = 4U};

    memcpy(message.payload, "ping", message.payload_size);
    return protocol_encode(&message, buffer, PROTOCOL_PACKET_MAX_SIZE, encoded_size);
}

static int decode_must_fail(const uint8_t *buffer, size_t size) {
    protocol_message_t decoded = {0};
    return !protocol_decode(buffer, size, &decoded);
}

static int test_invalid_encode(void) {
    protocol_message_t message = {.type = ECHO_REQUEST};
    uint8_t buffer[PROTOCOL_PACKET_MAX_SIZE];
    size_t encoded_size = 99U;

    if (protocol_encode(NULL, buffer, sizeof(buffer), &encoded_size)) return 0;
    if (encoded_size != 0) return 0;
    if (protocol_encode(&message, NULL, sizeof(buffer), &encoded_size)) return 0;
    if (protocol_encode(&message, buffer, sizeof(buffer), NULL)) return 0;

    message.payload_size = PROTOCOL_PAYLOAD_MAX + 1U;
    if (protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    message.payload_size = 1U;
    if (protocol_encode(&message, buffer, PROTOCOL_HEADER_SIZE, &encoded_size)) return 0;

    message.type = ECHO_REQUEST;
    message.counter = 1U;
    if (protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    message.type = ECHO_RESPONSE;
    message.counter = 0U;
    if (protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    message.type = ECHO_CLOSE;
    message.payload_size = 1U;
    if (protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    return 1;
}

static int test_invalid_decode(void) {
    uint8_t original[PROTOCOL_PACKET_MAX_SIZE];
    uint8_t changed[PROTOCOL_PACKET_MAX_SIZE];
    size_t size = 0;

    if (!encode_request(original, &size)) return 0;
    if (!decode_must_fail(NULL, size)) return 0;
    if (!decode_must_fail(original, PROTOCOL_HEADER_SIZE - 1U)) return 0;
    if (!decode_must_fail(original, size - 1U)) return 0;

    memcpy(changed, original, size);
    changed[MAGIC_OFFSET] ^= UINT8_C(1);
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    changed[VERSION_OFFSET] = PROTOCOL_VERSION + UINT8_C(1);
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    changed[TYPE_OFFSET] = UINT8_C(255);
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    changed[PAYLOAD_LENGTH_OFFSET + 1U] = UINT8_C(5);
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    changed[COUNTER_OFFSET + 3U] = UINT8_C(1);
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    changed[TYPE_OFFSET] = ECHO_CLOSE;
    if (!decode_must_fail(changed, size)) return 0;

    return 1;
}

int main(void) {
    if (!test_expected_bytes()) {
        fprintf(stderr, "Expected byte sequence test failed\n");
        return 1;
    }
    if (!test_round_trip(ECHO_REQUEST, 0U, "hello")) {
        fprintf(stderr, "ECHO_REQUEST round-trip test failed\n");
        return 1;
    }
    if (!test_round_trip(ECHO_RESPONSE, 42U, "hello")) {
        fprintf(stderr, "ECHO_RESPONSE round-trip test failed\n");
        return 1;
    }
    if (!test_round_trip(ECHO_CLOSE, 0U, "")) {
        fprintf(stderr, "ECHO_CLOSE round-trip test failed\n");
        return 1;
    }
    if (!test_invalid_encode()) {
        fprintf(stderr, "Invalid encode test failed\n");
        return 1;
    }
    if (!test_invalid_decode()) {
        fprintf(stderr, "Invalid decode test failed\n");
        return 1;
    }

    puts("Protocol tests passed");
    return 0;
}
