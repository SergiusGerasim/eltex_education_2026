#pragma once

#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    int descriptor;
    struct sockaddr_in broadcast_address;
} udp_socket_t;

bool udp_socket_open(udp_socket_t *udp_socket, const char *broadcast_address, uint16_t port);
void udp_socket_close(udp_socket_t *udp_socket);
ssize_t udp_socket_send(const udp_socket_t *udp_socket, const void *data, size_t size);
ssize_t udp_socket_receive(const udp_socket_t *udp_socket, void *buffer, size_t capacity, struct sockaddr_in *sender_address);
