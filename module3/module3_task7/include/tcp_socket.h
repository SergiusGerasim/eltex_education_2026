#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    int descriptor;
} tcp_socket_t;

void tcp_socket_init(tcp_socket_t *tcp_socket);
bool tcp_socket_connect(tcp_socket_t *tcp_socket, const char *server_address, uint16_t port);
bool tcp_socket_listen(tcp_socket_t *tcp_socket, const char *listen_address, uint16_t port, int backlog);
bool tcp_socket_accept(const tcp_socket_t *listener, tcp_socket_t *client);
bool tcp_socket_set_nonblocking(tcp_socket_t *tcp_socket);
ssize_t tcp_socket_send(const tcp_socket_t *tcp_socket, const void *data, size_t size);
ssize_t tcp_socket_receive(const tcp_socket_t *tcp_socket, void *buffer, size_t capacity);
void tcp_socket_close(tcp_socket_t *tcp_socket);
