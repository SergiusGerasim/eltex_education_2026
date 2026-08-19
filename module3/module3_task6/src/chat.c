#include "chat.h"
#include "signal_handler.h"
#include "protocol.h"
#include "udp_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <sys/random.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>

static bool generate_sender_id(uint64_t *sender_id){
    if (sender_id == NULL) {
        errno = EINVAL;
        return false;
    }
    do{
        unsigned char *bytes = (unsigned char *)sender_id;

        size_t total_count_byte = 0;
        while (total_count_byte < sizeof(uint64_t))
        {
            ssize_t get_bytes = getrandom(bytes + total_count_byte, sizeof(*sender_id) - total_count_byte, 0);
            if (get_bytes == -1){
                if (errno == EINTR){
                    continue;
                }
                else {
                    return false;
                }
            }
            if (get_bytes == 0){
                errno = EIO;
                return false;
            }

            total_count_byte += (size_t)get_bytes;
        }
    } while (*sender_id == 0);
    return true;
}

static bool send_chat_message(const udp_socket_t *udp_socket, const chat_message_t *message){
    uint8_t packet[CHAT_PACKET_MAX_SIZE];
    size_t packet_size = 0;

    if (!protocol_encode(message, packet, sizeof(packet), &packet_size)) {
        errno = EINVAL;
        return false;
    }

    const ssize_t sent = udp_socket_send(udp_socket, packet, packet_size);
    if (sent == -1) return false;
    if ((size_t)sent != packet_size) {
        errno = EIO;
        return false;
    }

    return true;
}

static bool receive_chat_message(const udp_socket_t *udp_socket, uint64_t sender_id) {
    uint8_t packet[CHAT_PACKET_MAX_SIZE + 1U];
    const ssize_t received = udp_socket_receive(udp_socket, packet, sizeof(packet), NULL);

    if (received == -1) return false;
    if ((size_t)received > CHAT_PACKET_MAX_SIZE) return true;

    chat_message_t message;
    if (!protocol_decode(packet, (size_t)received, &message)) return true;
    if (message.sender_id == sender_id) return true;

    if (message.type == CHAT_PACKET_JOIN) printf("%s joined the chat\n", message.sender_name);
    else if (message.type == CHAT_PACKET_LEAVE) printf("%s left the chat\n", message.sender_name);
    else if (message.type == CHAT_PACKET_MESSAGE) printf("%s: %s\n", message.sender_name, message.text);

    return true;
}

static void discard_stdin_line(void) {
    int character;

    do {
        character = getchar();
    } while (character != '\n' && character != EOF);
}

int chat_run(const chat_config_t *config) {
    if (config == NULL) return EXIT_FAILURE;
    if (config->name == NULL) {
        errno = EINVAL;
        perror("client name");
        return EXIT_FAILURE;
    }

    const size_t name_size = strnlen(config->name, CHAT_NAME_MAX + 1U);
    if (name_size == 0 || name_size > CHAT_NAME_MAX) {
        errno = EINVAL;
        perror("client name");
        return EXIT_FAILURE;
    }

    if (signal_handler_install() == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    uint64_t sender_id;
    if (!generate_sender_id(&sender_id)){
        perror("getrandom");
        return EXIT_FAILURE;
    }
    udp_socket_t udp_socket;
    if(!udp_socket_open(&udp_socket, config->broadcast_address, config->port)){
        perror("udp_socket_open");
        return EXIT_FAILURE;
    }

    chat_message_t join_message = {0};
    join_message.sender_id = sender_id;
    join_message.type = CHAT_PACKET_JOIN;
    memcpy(join_message.sender_name, config->name, name_size);
    join_message.sender_name[name_size] = '\0';

    if (!send_chat_message(&udp_socket, &join_message)) {
        perror("send JOIN");
        udp_socket_close(&udp_socket);
        return EXIT_FAILURE;
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);

    enum {
        SOCKET_EVENT,
        STDIN_EVENT,
        EVENT_COUNT
    };
    struct pollfd events[EVENT_COUNT] = {
        [SOCKET_EVENT] = {.fd = udp_socket.descriptor, .events = POLLIN},
        [STDIN_EVENT] = {.fd = STDIN_FILENO, .events = POLLIN}
    };
    int result = EXIT_SUCCESS;
    bool running = true;

    while (running && signal_handler_stop_requested() == 0) {
        const int poll_result = poll(events, EVENT_COUNT, -1);

        if (poll_result == -1) {
            if (errno == EINTR && signal_handler_stop_requested() != 0) break;
            if (errno == EINTR) continue;
            perror("poll");
            result = EXIT_FAILURE;
            break;
        }

        if (events[SOCKET_EVENT].revents & (POLLERR | POLLNVAL)) {
            fprintf(stderr, "UDP socket polling error\n");
            result = EXIT_FAILURE;
            break;
        }

        if ((events[SOCKET_EVENT].revents & POLLIN) && !receive_chat_message(&udp_socket, sender_id)) {
            perror("udp receive");
            result = EXIT_FAILURE;
            break;
        }

        if (events[STDIN_EVENT].revents & (POLLERR | POLLNVAL)) {
            fprintf(stderr, "Standard input polling error\n");
            result = EXIT_FAILURE;
            break;
        }

        if (events[STDIN_EVENT].revents & POLLIN) {
            char input[CHAT_TEXT_MAX + 2U];

            errno = 0;
            if (fgets(input, sizeof(input), stdin) == NULL) {
                if (feof(stdin)) {
                    running = false;
                    continue;
                }
                if (errno == EINTR) {
                    clearerr(stdin);
                    continue;
                }
                perror("stdin");
                result = EXIT_FAILURE;
                break;
            }

            size_t input_size = strlen(input);
            const bool has_newline = input_size > 0 && input[input_size - 1U] == '\n';
            const bool input_ended = feof(stdin);

            if (has_newline) input[--input_size] = '\0';
            if (!has_newline && input_size > CHAT_TEXT_MAX) {
                discard_stdin_line();
                fprintf(stderr, "Message is too long; maximum length is %u bytes\n", CHAT_TEXT_MAX);
                continue;
            }
            if (input_size == 0) {
                if (input_ended) running = false;
                continue;
            }
            if (strcmp(input, "/exit") == 0) {
                running = false;
                continue;
            }

            chat_message_t outgoing_message = join_message;
            outgoing_message.type = CHAT_PACKET_MESSAGE;
            memcpy(outgoing_message.text, input, input_size);
            outgoing_message.text[input_size] = '\0';

            if (!send_chat_message(&udp_socket, &outgoing_message)) {
                perror("send MESSAGE");
                result = EXIT_FAILURE;
                break;
            }

            if (input_ended) running = false;
        } else if (events[STDIN_EVENT].revents & POLLHUP) {
            running = false;
        }
    }
    chat_message_t leave_message = {0};
    leave_message.type = CHAT_PACKET_LEAVE;
    leave_message.sender_id = sender_id;
    memcpy(leave_message.sender_name, config->name, name_size);
    leave_message.sender_name[name_size] = '\0';

    bool send_leave = send_chat_message(&udp_socket, &leave_message);

    udp_socket_close(&udp_socket);
    if (!send_leave) {
        perror("send_leave");
        return EXIT_FAILURE;
    }
    return result;
}
