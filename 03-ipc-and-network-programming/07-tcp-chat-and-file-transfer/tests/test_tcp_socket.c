#include "tcp_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static int socket_is_nonblocking(const tcp_socket_t *tcp_socket) {
    const int flags = fcntl(tcp_socket->descriptor, F_GETFL);
    return flags != -1 && (flags & O_NONBLOCK) != 0;
}

static int listener_port(const tcp_socket_t *listener, uint16_t *port) {
    struct sockaddr_in address;
    socklen_t address_size = sizeof(address);

    if (getsockname(listener->descriptor, (struct sockaddr *)&address, &address_size) == -1) return 0;
    *port = ntohs(address.sin_port);
    return *port != 0;
}

static int wait_for_event(int descriptor, short requested_events) {
    struct pollfd event = {.fd = descriptor, .events = requested_events};
    int status;

    do {
        status = poll(&event, 1, 1000);
    } while (status == -1 && errno == EINTR);
    return status == 1 && (event.revents & requested_events) != 0;
}

static int send_all(const tcp_socket_t *tcp_socket, const void *data, size_t size) {
    const unsigned char *bytes = data;
    size_t offset = 0;

    while (offset < size) {
        if (!wait_for_event(tcp_socket->descriptor, POLLOUT)) return 0;
        const ssize_t sent = tcp_socket_send(tcp_socket, bytes + offset, size - offset);
        if (sent > 0) offset += (size_t)sent;
        else if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK) return 0;
    }
    return 1;
}

static int receive_all(const tcp_socket_t *tcp_socket, void *buffer, size_t size) {
    unsigned char *bytes = buffer;
    size_t offset = 0;

    while (offset < size) {
        if (!wait_for_event(tcp_socket->descriptor, POLLIN)) return 0;
        const ssize_t received = tcp_socket_receive(tcp_socket, bytes + offset, size - offset);
        if (received > 0) offset += (size_t)received;
        else if (received == 0 || (received == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) return 0;
    }
    return 1;
}

static int test_loopback_connection(void) {
    static const char expected[] = "TCP loopback";
    tcp_socket_t listener;
    tcp_socket_t sender;
    tcp_socket_t receiver;
    uint16_t port;
    char buffer[sizeof(expected)] = {0};
    int result = 0;

    tcp_socket_init(&listener);
    tcp_socket_init(&sender);
    tcp_socket_init(&receiver);

    if (!tcp_socket_listen(&listener, "127.0.0.1", 0, 4)) { perror("listen"); goto cleanup; }
    if (!listener_port(&listener, &port)) { perror("getsockname"); goto cleanup; }
    if (!tcp_socket_connect(&sender, "127.0.0.1", port)) { perror("connect"); goto cleanup; }
    if (!wait_for_event(listener.descriptor, POLLIN)) { perror("poll listener"); goto cleanup; }
    if (!tcp_socket_accept(&listener, &receiver)) { perror("accept"); goto cleanup; }
    if (!socket_is_nonblocking(&listener) || !socket_is_nonblocking(&sender) || !socket_is_nonblocking(&receiver)) { perror("nonblocking"); goto cleanup; }

    if (!send_all(&sender, expected, sizeof(expected))) { perror("send"); goto cleanup; }
    if (!receive_all(&receiver, buffer, sizeof(buffer))) { perror("receive"); goto cleanup; }
    if (memcmp(buffer, expected, sizeof(expected)) != 0) { fprintf(stderr, "payload mismatch\n"); goto cleanup; }

    errno = 0;
    if (tcp_socket_receive(&receiver, buffer, sizeof(buffer)) != -1) { fprintf(stderr, "empty receive unexpectedly succeeded\n"); goto cleanup; }
    if (errno != EAGAIN && errno != EWOULDBLOCK) { perror("empty receive"); goto cleanup; }
    result = 1;

cleanup:
    tcp_socket_close(&receiver);
    tcp_socket_close(&sender);
    tcp_socket_close(&listener);
    return result;
}

static int test_invalid_arguments(void) {
    tcp_socket_t tcp_socket;
    tcp_socket_init(&tcp_socket);

    if (tcp_socket_connect(&tcp_socket, "invalid", 5000)) return 0;
    if (tcp_socket_listen(&tcp_socket, "127.0.0.1", 5000, 0)) return 0;
    if (tcp_socket_send(&tcp_socket, "x", 1) != -1) return 0;
    return tcp_socket_receive(&tcp_socket, &tcp_socket, sizeof(tcp_socket)) == -1;
}

int main(void) {
    if (!test_loopback_connection()) {
        fprintf(stderr, "TCP loopback test failed\n");
        return 1;
    }
    if (!test_invalid_arguments()) {
        fprintf(stderr, "TCP invalid argument test failed\n");
        return 1;
    }

    puts("TCP socket tests passed");
    return 0;
}
