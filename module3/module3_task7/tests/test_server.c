#include "protocol.h"
#include "tcp_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int find_free_port(uint16_t *port) {
    const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1) return 0;

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    int result = bind(descriptor, (const struct sockaddr *)&address, sizeof(address)) != -1;

    socklen_t address_size = sizeof(address);
    if (result) result = getsockname(descriptor, (struct sockaddr *)&address, &address_size) != -1;
    if (result) *port = ntohs(address.sin_port);
    close(descriptor);
    return result && *port != 0;
}

static int wait_for_event(int descriptor, short requested_events) {
    struct pollfd event = {.fd = descriptor, .events = requested_events};
    int status;

    do {
        status = poll(&event, 1, 2000);
    } while (status == -1 && errno == EINTR);
    return status == 1 && (event.revents & requested_events) != 0;
}

static int connect_with_retry(tcp_socket_t *client, uint16_t port) {
    const struct timespec delay = {.tv_nsec = 10000000L};

    for (int attempt = 0; attempt < 100; ++attempt) {
        if (tcp_socket_connect(client, "127.0.0.1", port)) return 1;
        nanosleep(&delay, NULL);
    }
    return 0;
}

static int send_frame(const tcp_socket_t *client, const chat_message_t *message) {
    uint8_t frame[CHAT_PACKET_MAX_SIZE];
    size_t frame_size;
    size_t offset = 0;

    if (!protocol_encode(message, frame, sizeof(frame), &frame_size)) return 0;
    while (offset < frame_size) {
        if (!wait_for_event(client->descriptor, POLLOUT)) return 0;
        const ssize_t sent = tcp_socket_send(client, frame + offset, frame_size - offset);
        if (sent > 0) offset += (size_t)sent;
        else if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK) return 0;
    }
    return 1;
}

static int receive_frame(const tcp_socket_t *client, chat_message_t *message) {
    uint8_t frame[CHAT_PACKET_MAX_SIZE];
    size_t received_size = 0;
    size_t frame_size = 0;

    for (;;) {
        const protocol_frame_status_t status = protocol_frame_size(frame, received_size, &frame_size);
        if (status == PROTOCOL_FRAME_COMPLETE) return protocol_decode(frame, frame_size, message);
        if (status == PROTOCOL_FRAME_INVALID || received_size == sizeof(frame)) return 0;
        if (!wait_for_event(client->descriptor, POLLIN)) return 0;

        const ssize_t received = tcp_socket_receive(client, frame + received_size, sizeof(frame) - received_size);
        if (received > 0) received_size += (size_t)received;
        else if (received == 0 || (received == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) return 0;
    }
}

static chat_message_t make_message(chat_packet_type_t type, uint64_t sender_id, const char *name, const char *text) {
    chat_message_t message = {.type = type, .sender_id = sender_id};
    strcpy(message.sender_name, name);
    if (text != NULL) strcpy(message.text, text);
    return message;
}

static int run_scenario(uint16_t port) {
    tcp_socket_t alice;
    tcp_socket_t bob;
    chat_message_t received;
    int result = 0;

    tcp_socket_init(&alice);
    tcp_socket_init(&bob);
    if (!connect_with_retry(&alice, port)) goto cleanup;

    const chat_message_t alice_join = make_message(CHAT_PACKET_JOIN, 1, "Alice", NULL);
    if (!send_frame(&alice, &alice_join)) goto cleanup;
    if (!connect_with_retry(&bob, port)) goto cleanup;

    const chat_message_t bob_join = make_message(CHAT_PACKET_JOIN, 2, "Bob", NULL);
    if (!send_frame(&bob, &bob_join)) goto cleanup;
    if (!receive_frame(&alice, &received)) goto cleanup;
    if (received.type != CHAT_PACKET_JOIN || strcmp(received.sender_name, "Bob") != 0) goto cleanup;

    const chat_message_t text = make_message(CHAT_PACKET_MESSAGE, 1, "Alice", "Hello, Bob");
    if (!send_frame(&alice, &text)) goto cleanup;
    if (!receive_frame(&bob, &received)) goto cleanup;
    if (received.type != CHAT_PACKET_MESSAGE || strcmp(received.text, "Hello, Bob") != 0) goto cleanup;

    tcp_socket_close(&alice);
    if (!receive_frame(&bob, &received)) goto cleanup;
    if (received.type != CHAT_PACKET_LEAVE || strcmp(received.sender_name, "Alice") != 0) goto cleanup;
    result = 1;

cleanup:
    tcp_socket_close(&bob);
    tcp_socket_close(&alice);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) return EXIT_FAILURE;

    uint16_t port;
    if (!find_free_port(&port)) {
        perror("find_free_port");
        return EXIT_FAILURE;
    }

    char port_text[6];
    snprintf(port_text, sizeof(port_text), "%u", port);
    const pid_t server_pid = fork();
    if (server_pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (server_pid == 0) {
        execl(argv[1], argv[1], "127.0.0.1", port_text, (char *)NULL);
        _exit(127);
    }

    const int scenario_succeeded = run_scenario(port);
    kill(server_pid, SIGINT);

    int server_status;
    if (waitpid(server_pid, &server_status, 0) == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    if (!scenario_succeeded || !WIFEXITED(server_status) || WEXITSTATUS(server_status) != EXIT_SUCCESS) {
        fprintf(stderr, "Server integration test failed\n");
        return EXIT_FAILURE;
    }

    puts("Server integration tests passed");
    return EXIT_SUCCESS;
}
