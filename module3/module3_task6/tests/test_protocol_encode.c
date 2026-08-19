#include "protocol.h"

#include <stdio.h>
#include <string.h>

#define MAGIC_OFFSET 0U
#define VERSION_OFFSET 4U
#define TYPE_OFFSET 5U
#define SENDER_ID_OFFSET 6U
#define NAME_LENGTH_OFFSET 14U
#define TEXT_LENGTH_OFFSET 16U

static int check_message(const chat_message_t *actual, const chat_message_t *expected) {
    if (actual->type != expected->type) return 0;
    if (actual->sender_id != expected->sender_id) return 0;
    if (strcmp(actual->sender_name, expected->sender_name) != 0) return 0;
    return strcmp(actual->text, expected->text) == 0;
}

static int test_expected_bytes(void) {
    static const uint8_t expected[] = {
        0x4D, 0x33, 0x54, 0x36,
        0x01,
        0x02,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B,
        0x00, 0x05,
        0x00, 0x05,
        0x41, 0x6C, 0x69, 0x63, 0x65,
        0x48, 0x65, 0x6C, 0x6C, 0x6F
    };
    chat_message_t message = {.type = CHAT_PACKET_MESSAGE, .sender_id = UINT64_C(123)};
    uint8_t buffer[CHAT_PACKET_MAX_SIZE];
    size_t encoded_size = 0;

    strcpy(message.sender_name, "Alice");
    strcpy(message.text, "Hello");

    if (!protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;
    if (encoded_size != sizeof(expected)) return 0;
    return memcmp(buffer, expected, sizeof(expected)) == 0;
}

static int test_round_trip(chat_packet_type_t type, const char *text) {
    chat_message_t source = {.type = type, .sender_id = UINT64_C(0x1122334455667788)};
    chat_message_t decoded = {0};
    uint8_t buffer[CHAT_PACKET_MAX_SIZE];
    size_t encoded_size = 0;

    strcpy(source.sender_name, "Alice");
    strcpy(source.text, text);

    if (!protocol_encode(&source, buffer, sizeof(buffer), &encoded_size)) return 0;
    if (!protocol_decode(buffer, encoded_size, &decoded)) return 0;
    return check_message(&decoded, &source);
}

static int encode_message_packet(uint8_t *buffer, size_t *encoded_size) {
    chat_message_t message = {.type = CHAT_PACKET_MESSAGE, .sender_id = UINT64_C(123)};

    strcpy(message.sender_name, "Alice");
    strcpy(message.text, "Hello");
    return protocol_encode(&message, buffer, CHAT_PACKET_MAX_SIZE, encoded_size);
}

static int decode_must_fail(uint8_t *buffer, size_t size) {
    chat_message_t decoded = {0};

    return !protocol_decode(buffer, size, &decoded);
}

static int test_invalid_packets(void) {
    uint8_t original[CHAT_PACKET_MAX_SIZE];
    uint8_t changed[CHAT_PACKET_MAX_SIZE];
    size_t size = 0;

    if (!encode_message_packet(original, &size)) return 0;

    memcpy(changed, original, size);
    changed[MAGIC_OFFSET] ^= UINT8_C(1);
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    changed[VERSION_OFFSET] = CHAT_PROTOCOL_VERSION + UINT8_C(1);
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    changed[TYPE_OFFSET] = UINT8_C(255);
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    memset(changed + SENDER_ID_OFFSET, 0, sizeof(uint64_t));
    if (!decode_must_fail(changed, size)) return 0;

    if (!decode_must_fail(original, CHAT_PACKET_HEADER_SIZE - 1U)) return 0;
    if (!decode_must_fail(original, size - 1U)) return 0;

    memcpy(changed, original, size);
    changed[NAME_LENGTH_OFFSET] = 0;
    changed[NAME_LENGTH_OFFSET + 1U] = 0;
    if (!decode_must_fail(changed, size)) return 0;

    memcpy(changed, original, size);
    changed[TEXT_LENGTH_OFFSET] = 0;
    changed[TEXT_LENGTH_OFFSET + 1U] = 0;
    if (!decode_must_fail(changed, CHAT_PACKET_HEADER_SIZE + 5U)) return 0;

    memcpy(changed, original, size);
    changed[TYPE_OFFSET] = CHAT_PACKET_JOIN;
    if (!decode_must_fail(changed, size)) return 0;

    return 1;
}

static int test_invalid_encode_arguments(void) {
    chat_message_t message = {.type = CHAT_PACKET_MESSAGE, .sender_id = UINT64_C(123)};
    uint8_t buffer[CHAT_PACKET_MAX_SIZE];
    size_t encoded_size = 99;

    strcpy(message.sender_name, "Alice");
    strcpy(message.text, "Hello");

    if (protocol_encode(NULL, buffer, sizeof(buffer), &encoded_size)) return 0;
    if (encoded_size != 0) return 0;
    if (protocol_encode(&message, buffer, CHAT_PACKET_HEADER_SIZE, &encoded_size)) return 0;

    message.sender_id = 0;
    if (protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    message.sender_id = 123;
    message.text[0] = '\0';
    if (protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    return 1;
}

int main(void) {
    if (!test_expected_bytes()) {
        fprintf(stderr, "Expected byte sequence test failed\n");
        return 1;
    }

    if (!test_round_trip(CHAT_PACKET_JOIN, "")) {
        fprintf(stderr, "JOIN round-trip test failed\n");
        return 1;
    }

    if (!test_round_trip(CHAT_PACKET_MESSAGE, "Hello")) {
        fprintf(stderr, "MESSAGE round-trip test failed\n");
        return 1;
    }

    if (!test_round_trip(CHAT_PACKET_LEAVE, "")) {
        fprintf(stderr, "LEAVE round-trip test failed\n");
        return 1;
    }

    if (!test_invalid_packets()) {
        fprintf(stderr, "Invalid packet test failed\n");
        return 1;
    }

    if (!test_invalid_encode_arguments()) {
        fprintf(stderr, "Invalid encode argument test failed\n");
        return 1;
    }

    puts("Protocol tests passed");
    return 0;
}
