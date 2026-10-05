#include "server.h"
#include "protocol.h"
#include "signal_handler.h"
#include "tcp_socket.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define SERVER_BACKLOG 32
#define SERVER_MAX_CLIENTS 64U
#define SERVER_MAX_EVENTS 32
#define SERVER_OUTPUT_CAPACITY (CHAT_PACKET_MAX_SIZE * 4U)
#define LISTENER_EVENT_ID UINT32_MAX

typedef struct {
    tcp_socket_t socket;
    bool active;
    bool joined;
    uint64_t sender_id;
    char sender_name[CHAT_NAME_MAX + 1U];
    bool transfer_active;
    uint64_t transfer_id;
    uint64_t transfer_size;
    uint64_t transferred_size;
    uint8_t input[CHAT_PACKET_MAX_SIZE];
    size_t input_size;
    uint8_t output[SERVER_OUTPUT_CAPACITY];
    size_t output_offset;
    size_t output_size;
} server_client_t;

typedef struct {
    tcp_socket_t listener;
    int epoll_descriptor;
    server_client_t clients[SERVER_MAX_CLIENTS];
} server_state_t;

static uint32_t client_event_id(size_t index) {
    return (uint32_t)index;
}

static uint32_t client_events(const server_client_t *client) {
    uint32_t events = EPOLLIN | EPOLLRDHUP;
    if (client->output_offset < client->output_size) events |= EPOLLOUT;
    return events;
}

static bool update_client_events(server_state_t *server, size_t index) {
    struct epoll_event event = {
        .events = client_events(&server->clients[index]),
        .data.u32 = client_event_id(index)
    };
    return epoll_ctl(server->epoll_descriptor, EPOLL_CTL_MOD, server->clients[index].socket.descriptor, &event) != -1;
}

static void close_client(server_state_t *server, size_t index) {
    server_client_t *client = &server->clients[index];
    if (!client->active) return;

    epoll_ctl(server->epoll_descriptor, EPOLL_CTL_DEL, client->socket.descriptor, NULL);
    tcp_socket_close(&client->socket);
    memset(client, 0, sizeof(*client));
    tcp_socket_init(&client->socket);
}

static bool queue_frame(server_state_t *server, size_t index, const uint8_t *frame, size_t frame_size) {
    server_client_t *client = &server->clients[index];
    const size_t pending_size = client->output_size - client->output_offset;

    if (client->output_offset != 0 && pending_size != 0) memmove(client->output, client->output + client->output_offset, pending_size);
    client->output_offset = 0;
    client->output_size = pending_size;
    if (frame_size > sizeof(client->output) - client->output_size) return false;

    memcpy(client->output + client->output_size, frame, frame_size);
    client->output_size += frame_size;
    return update_client_events(server, index);
}

static void broadcast_frame(server_state_t *server, size_t excluded_index, const uint8_t *frame, size_t frame_size) {
    for (size_t index = 0; index < SERVER_MAX_CLIENTS; ++index) {
        if (index == excluded_index || !server->clients[index].active || !server->clients[index].joined) continue;
        if (!queue_frame(server, index, frame, frame_size)) close_client(server, index);
    }
}

static void announce_leave(server_state_t *server, size_t index) {
    const server_client_t *client = &server->clients[index];
    if (!client->joined) return;

    chat_message_t leave = {
        .type = CHAT_PACKET_LEAVE,
        .sender_id = client->sender_id
    };
    uint8_t frame[CHAT_PACKET_MAX_SIZE];
    size_t frame_size;

    memcpy(leave.sender_name, client->sender_name, sizeof(leave.sender_name));
    if (protocol_encode(&leave, frame, sizeof(frame), &frame_size)) broadcast_frame(server, index, frame, frame_size);
}

static void disconnect_client(server_state_t *server, size_t index, bool announce) {
    if (announce) announce_leave(server, index);
    close_client(server, index);
}

static bool client_identity_matches(const server_client_t *client, const chat_message_t *message) {
    return client->sender_id == message->sender_id && strcmp(client->sender_name, message->sender_name) == 0;
}

static bool validate_file_transfer(server_client_t *client, const chat_message_t *message) {
    if (message->type == CHAT_PACKET_FILE_BEGIN) {
        if (client->transfer_active) return false;
        client->transfer_active = true;
        client->transfer_id = message->transfer_id;
        client->transfer_size = message->file_size;
        client->transferred_size = 0;
        return true;
    }
    if (message->type == CHAT_PACKET_FILE_CHUNK) {
        if (!client->transfer_active || message->transfer_id != client->transfer_id) return false;
        if (message->file_data_size > client->transfer_size - client->transferred_size) return false;
        client->transferred_size += message->file_data_size;
        return true;
    }
    if (message->type == CHAT_PACKET_FILE_END) {
        if (!client->transfer_active || message->transfer_id != client->transfer_id) return false;
        if (client->transferred_size != client->transfer_size) return false;
        client->transfer_active = false;
        client->transfer_id = 0;
        client->transfer_size = 0;
        client->transferred_size = 0;
    }
    return true;
}

static bool handle_message(server_state_t *server, size_t index, const uint8_t *frame, size_t frame_size) {
    server_client_t *client = &server->clients[index];
    chat_message_t message;
    if (!protocol_decode(frame, frame_size, &message)) return false;

    if (!client->joined) {
        if (message.type != CHAT_PACKET_JOIN) return false;
        client->joined = true;
        client->sender_id = message.sender_id;
        memcpy(client->sender_name, message.sender_name, sizeof(client->sender_name));
        printf("%s joined the chat\n", client->sender_name);
        broadcast_frame(server, index, frame, frame_size);
        return true;
    }

    if (!client_identity_matches(client, &message) || message.type == CHAT_PACKET_JOIN) return false;
    if (!validate_file_transfer(client, &message)) return false;
    if (message.type == CHAT_PACKET_LEAVE) {
        broadcast_frame(server, index, frame, frame_size);
        printf("%s left the chat\n", client->sender_name);
        close_client(server, index);
        return false;
    }

    broadcast_frame(server, index, frame, frame_size);
    return true;
}

static bool process_input(server_state_t *server, size_t index) {
    server_client_t *client = &server->clients[index];

    while (client->input_size != 0) {
        size_t frame_size;
        const protocol_frame_status_t status = protocol_frame_size(client->input, client->input_size, &frame_size);
        if (status == PROTOCOL_FRAME_INVALID) return false;
        if (status == PROTOCOL_FRAME_INCOMPLETE) return true;
        if (!handle_message(server, index, client->input, frame_size)) return false;
        if (!client->active) return false;

        const size_t remaining_size = client->input_size - frame_size;
        memmove(client->input, client->input + frame_size, remaining_size);
        client->input_size = remaining_size;
    }
    return true;
}

static bool receive_client_data(server_state_t *server, size_t index) {
    server_client_t *client = &server->clients[index];

    for (;;) {
        if (client->input_size == sizeof(client->input)) return false;
        const ssize_t received = tcp_socket_receive(&client->socket, client->input + client->input_size, sizeof(client->input) - client->input_size);

        if (received > 0) {
            client->input_size += (size_t)received;
            if (!process_input(server, index)) return false;
            if (!client->active) return false;
            continue;
        }
        if (received == 0) return false;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
        return false;
    }
}

static bool flush_client_output(server_state_t *server, size_t index) {
    server_client_t *client = &server->clients[index];

    while (client->output_offset < client->output_size) {
        const ssize_t sent = tcp_socket_send(&client->socket, client->output + client->output_offset, client->output_size - client->output_offset);
        if (sent > 0) {
            client->output_offset += (size_t)sent;
            continue;
        }
        if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        return false;
    }

    client->output_offset = 0;
    client->output_size = 0;
    return update_client_events(server, index);
}

static size_t find_free_client(const server_state_t *server) {
    for (size_t index = 0; index < SERVER_MAX_CLIENTS; ++index) {
        if (!server->clients[index].active) return index;
    }
    return SERVER_MAX_CLIENTS;
}

static void accept_clients(server_state_t *server) {
    for (;;) {
        tcp_socket_t socket;
        tcp_socket_init(&socket);
        if (!tcp_socket_accept(&server->listener, &socket)) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) perror("accept");
            return;
        }

        const size_t index = find_free_client(server);
        if (index == SERVER_MAX_CLIENTS) {
            tcp_socket_close(&socket);
            continue;
        }

        server_client_t *client = &server->clients[index];
        memset(client, 0, sizeof(*client));
        client->socket = socket;
        client->active = true;

        struct epoll_event event = {
            .events = client_events(client),
            .data.u32 = client_event_id(index)
        };
        if (epoll_ctl(server->epoll_descriptor, EPOLL_CTL_ADD, socket.descriptor, &event) == -1) {
            perror("epoll_ctl client");
            close_client(server, index);
        }
    }
}

static void initialize_clients(server_state_t *server) {
    for (size_t index = 0; index < SERVER_MAX_CLIENTS; ++index) tcp_socket_init(&server->clients[index].socket);
}

static void cleanup_server(server_state_t *server) {
    for (size_t index = 0; index < SERVER_MAX_CLIENTS; ++index) close_client(server, index);
    if (server->epoll_descriptor != -1) close(server->epoll_descriptor);
    tcp_socket_close(&server->listener);
}

int server_run(const server_config_t *config) {
    if (config == NULL || config->listen_address == NULL || config->port == 0) return EXIT_FAILURE;
    if (signal_handler_install() == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    server_state_t *server = calloc(1, sizeof(*server));
    if (server == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }
    tcp_socket_init(&server->listener);
    server->epoll_descriptor = -1;
    initialize_clients(server);

    int result = EXIT_FAILURE;
    if (!tcp_socket_listen(&server->listener, config->listen_address, config->port, SERVER_BACKLOG)) {
        perror("tcp_socket_listen");
        goto cleanup;
    }

    server->epoll_descriptor = epoll_create1(EPOLL_CLOEXEC);
    if (server->epoll_descriptor == -1) {
        perror("epoll_create1");
        goto cleanup;
    }

    struct epoll_event listener_event = {
        .events = EPOLLIN,
        .data.u32 = LISTENER_EVENT_ID
    };
    if (epoll_ctl(server->epoll_descriptor, EPOLL_CTL_ADD, server->listener.descriptor, &listener_event) == -1) {
        perror("epoll_ctl listener");
        goto cleanup;
    }

    printf("Server listening on %s:%u\n", config->listen_address, config->port);
    result = EXIT_SUCCESS;
    while (signal_handler_stop_requested() == 0) {
        struct epoll_event events[SERVER_MAX_EVENTS];
        const int event_count = epoll_wait(server->epoll_descriptor, events, SERVER_MAX_EVENTS, -1);
        if (event_count == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            result = EXIT_FAILURE;
            break;
        }

        for (int event_index = 0; event_index < event_count; ++event_index) {
            if (events[event_index].data.u32 == LISTENER_EVENT_ID) {
                accept_clients(server);
                continue;
            }

            const size_t index = events[event_index].data.u32;
            if (index >= SERVER_MAX_CLIENTS || !server->clients[index].active) continue;
            const uint32_t event_flags = events[event_index].events;

            if ((event_flags & EPOLLIN) != 0 && !receive_client_data(server, index)) {
                if (server->clients[index].active) disconnect_client(server, index, true);
                continue;
            }
            if (!server->clients[index].active) continue;
            if ((event_flags & EPOLLOUT) != 0 && !flush_client_output(server, index)) {
                disconnect_client(server, index, true);
                continue;
            }
            if ((event_flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) disconnect_client(server, index, true);
        }
    }

cleanup:
    cleanup_server(server);
    free(server);
    return result;
}
