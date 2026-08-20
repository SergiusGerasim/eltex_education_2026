#include "protocol.h"

#include <stdio.h>
#include <string.h>

#define MAGIC_OFFSET 0U
#define BODY_SIZE_OFFSET 8U

static void initialize_message(chat_message_t *message, chat_packet_type_t type, uint64_t transfer_id) {
    memset(message, 0, sizeof(*message));
    message->type = type;
    message->sender_id = UINT64_C(0x1122334455667788);
    message->transfer_id = transfer_id;
    strcpy(message->sender_name, "Alice");
}

static int encode_and_decode(const chat_message_t *source, chat_message_t *decoded) {
    uint8_t buffer[CHAT_PACKET_MAX_SIZE];
    size_t encoded_size = 0;
    size_t frame_size = 0;

    if (!protocol_encode(source, buffer, sizeof(buffer), &encoded_size)) return 0;
    if (protocol_frame_size(buffer, encoded_size, &frame_size) != PROTOCOL_FRAME_COMPLETE) return 0;
    if (frame_size != encoded_size) return 0;
    return protocol_decode(buffer, encoded_size, decoded);
}

static int test_chat_packets(void) {
    chat_message_t source;
    chat_message_t decoded;

    initialize_message(&source, CHAT_PACKET_JOIN, 0);
    if (!encode_and_decode(&source, &decoded) || decoded.type != source.type) return 0;

    initialize_message(&source, CHAT_PACKET_MESSAGE, 0);
    strcpy(source.text, "Hello over TCP");
    if (!encode_and_decode(&source, &decoded)) return 0;
    if (strcmp(decoded.text, source.text) != 0) return 0;

    initialize_message(&source, CHAT_PACKET_LEAVE, 0);
    return encode_and_decode(&source, &decoded) && decoded.type == source.type;
}

static int test_file_packets(void) {
    chat_message_t source;
    chat_message_t decoded;
    const uint64_t transfer_id = UINT64_C(0xAABBCCDDEEFF0011);

    initialize_message(&source, CHAT_PACKET_FILE_BEGIN, transfer_id);
    strcpy(source.file_name, "image.bin");
    source.file_size = UINT64_C(123456789);
    if (!encode_and_decode(&source, &decoded)) return 0;
    if (decoded.transfer_id != transfer_id || decoded.file_size != source.file_size) return 0;
    if (strcmp(decoded.file_name, source.file_name) != 0) return 0;

    initialize_message(&source, CHAT_PACKET_FILE_CHUNK, transfer_id);
    source.file_data_size = 5;
    memcpy(source.file_data, "\x00\x01\x7f\x80\xff", source.file_data_size);
    if (!encode_and_decode(&source, &decoded)) return 0;
    if (decoded.file_data_size != source.file_data_size) return 0;
    if (memcmp(decoded.file_data, source.file_data, source.file_data_size) != 0) return 0;

    initialize_message(&source, CHAT_PACKET_FILE_END, transfer_id);
    return encode_and_decode(&source, &decoded) && decoded.transfer_id == transfer_id;
}

static int test_stream_framing(void) {
    chat_message_t message;
    uint8_t buffer[CHAT_PACKET_MAX_SIZE];
    size_t encoded_size = 0;
    size_t frame_size = 99;

    initialize_message(&message, CHAT_PACKET_MESSAGE, 0);
    strcpy(message.text, "fragmented");
    if (!protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    if (protocol_frame_size(buffer, CHAT_PACKET_HEADER_SIZE - 1U, &frame_size) != PROTOCOL_FRAME_INCOMPLETE) return 0;
    if (frame_size != 0) return 0;
    if (protocol_frame_size(buffer, CHAT_PACKET_HEADER_SIZE, &frame_size) != PROTOCOL_FRAME_INCOMPLETE) return 0;
    if (frame_size != encoded_size) return 0;
    return protocol_frame_size(buffer, encoded_size, &frame_size) == PROTOCOL_FRAME_COMPLETE;
}

static int test_invalid_packets(void) {
    chat_message_t message;
    chat_message_t decoded;
    uint8_t buffer[CHAT_PACKET_MAX_SIZE];
    size_t encoded_size = 0;

    initialize_message(&message, CHAT_PACKET_MESSAGE, 0);
    strcpy(message.text, "Hello");
    if (!protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    buffer[MAGIC_OFFSET] ^= UINT8_C(1);
    if (protocol_decode(buffer, encoded_size, &decoded)) return 0;
    buffer[MAGIC_OFFSET] ^= UINT8_C(1);

    buffer[BODY_SIZE_OFFSET + 3U]++;
    if (protocol_decode(buffer, encoded_size, &decoded)) return 0;

    initialize_message(&message, CHAT_PACKET_FILE_CHUNK, 0);
    message.file_data_size = 1;
    message.file_data[0] = 42;
    if (protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    initialize_message(&message, CHAT_PACKET_FILE_BEGIN, 1);
    if (protocol_encode(&message, buffer, sizeof(buffer), &encoded_size)) return 0;

    return protocol_frame_size(NULL, 1, &encoded_size) == PROTOCOL_FRAME_INVALID;
}

int main(void) {
    if (!test_chat_packets()) {
        fprintf(stderr, "Chat packet tests failed\n");
        return 1;
    }
    if (!test_file_packets()) {
        fprintf(stderr, "File packet tests failed\n");
        return 1;
    }
    if (!test_stream_framing()) {
        fprintf(stderr, "Stream framing tests failed\n");
        return 1;
    }
    if (!test_invalid_packets()) {
        fprintf(stderr, "Invalid packet tests failed\n");
        return 1;
    }

    puts("Protocol tests passed");
    return 0;
}
