#include "tcp_socket.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOOPBACK_ADDRESS "127.0.0.1"

static void test_initialization_and_validation(void) {
    tcp_socket_t tcp_socket = {.descriptor = 10};
    tcp_socket_init(&tcp_socket);
    assert(tcp_socket.descriptor == -1);

    uint16_t port = 0;
    char byte = 0;
    assert(!tcp_socket_connect(NULL, LOOPBACK_ADDRESS, UINT16_C(1)));
    assert(errno == EINVAL);
    assert(!tcp_socket_listen(&tcp_socket, "invalid", 0, 1));
    assert(errno == EINVAL);
    assert(!tcp_socket_get_local_port(&tcp_socket, &port));
    assert(errno == EINVAL);
    assert(tcp_socket_send(&tcp_socket, &byte, sizeof(byte)) == -1);
    assert(errno == EINVAL);
    assert(tcp_socket_receive(&tcp_socket, &byte, sizeof(byte)) == -1);
    assert(errno == EINVAL);
}

static void test_loopback_connection(void) {
    tcp_socket_t listener;
    tcp_socket_t sender;
    tcp_socket_t receiver;
    tcp_socket_init(&listener);
    tcp_socket_init(&sender);
    tcp_socket_init(&receiver);

    assert(tcp_socket_listen(&listener, LOOPBACK_ADDRESS, 0, 4));

    uint16_t port = 0;
    assert(tcp_socket_get_local_port(&listener, &port));
    assert(port != 0);
    assert(tcp_socket_connect(&sender, LOOPBACK_ADDRESS, port));
    assert(tcp_socket_accept(&listener, &receiver));

    const int listener_flags = fcntl(listener.descriptor, F_GETFL);
    const int sender_flags = fcntl(sender.descriptor, F_GETFL);
    const int receiver_flags = fcntl(receiver.descriptor, F_GETFL);
    assert(listener_flags != -1 && (listener_flags & O_NONBLOCK) != 0);
    assert(sender_flags != -1 && (sender_flags & O_NONBLOCK) != 0);
    assert(receiver_flags != -1 && (receiver_flags & O_NONBLOCK) != 0);

    static const char message[] = "taxi protocol";
    assert(tcp_socket_send(&sender, message, sizeof(message)) == (ssize_t)sizeof(message));

    char buffer[sizeof(message)] = {0};
    assert(tcp_socket_receive(&receiver, buffer, sizeof(buffer)) == (ssize_t)sizeof(buffer));
    assert(memcmp(buffer, message, sizeof(message)) == 0);

    tcp_socket_close(&receiver);
    tcp_socket_close(&sender);
    tcp_socket_close(&listener);
    assert(receiver.descriptor == -1);
    assert(sender.descriptor == -1);
    assert(listener.descriptor == -1);
}

static void test_empty_nonblocking_accept(void) {
    tcp_socket_t listener;
    tcp_socket_t client;
    tcp_socket_init(&listener);
    tcp_socket_init(&client);

    assert(tcp_socket_listen(&listener, LOOPBACK_ADDRESS, 0, 1));
    assert(!tcp_socket_accept(&listener, &client));
    assert(errno == EAGAIN || errno == EWOULDBLOCK);
    assert(client.descriptor == -1);

    tcp_socket_close(&listener);
}

int main(void) {
    test_initialization_and_validation();
    test_loopback_connection();
    test_empty_nonblocking_accept();
    puts("tcp socket tests passed");
    return 0;
}
