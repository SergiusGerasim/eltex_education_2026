#include "tcp_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static bool arguments_are_valid(const tcp_socket_t *tcp_socket, const char *address) {
    return tcp_socket != NULL && address != NULL;
}

static bool parse_address(const char *text, uint16_t port, struct sockaddr_in *address) {
    *address = (struct sockaddr_in){0};
    address->sin_family = AF_INET;
    address->sin_port = htons(port);

    const int status = inet_pton(AF_INET, text, &address->sin_addr);
    if (status == 1) return true;
    if (status == 0) errno = EINVAL;
    return false;
}

static bool enable_reuse_address(int descriptor) {
    const int enabled = 1;
    return setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) != -1;
}

static bool finish_open(tcp_socket_t *tcp_socket, int descriptor) {
    tcp_socket_t opened_socket = {.descriptor = descriptor};
    if (tcp_socket_set_nonblocking(&opened_socket)) {
        *tcp_socket = opened_socket;
        return true;
    }

    const int saved_errno = errno;
    close(descriptor);
    errno = saved_errno;
    return false;
}

void tcp_socket_init(tcp_socket_t *tcp_socket) {
    if (tcp_socket == NULL) return;
    tcp_socket->descriptor = -1;
}

bool tcp_socket_connect(tcp_socket_t *tcp_socket, const char *server_address, uint16_t port) {
    if (!arguments_are_valid(tcp_socket, server_address) || port == 0) {
        errno = EINVAL;
        return false;
    }
    tcp_socket_init(tcp_socket);

    struct sockaddr_in address;
    if (!parse_address(server_address, port, &address)) return false;

    const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1) return false;

    int status;
    do {
        status = connect(descriptor, (const struct sockaddr *)&address, sizeof(address));
    } while (status == -1 && errno == EINTR);

    if (status == -1) {
        const int saved_errno = errno;
        close(descriptor);
        errno = saved_errno;
        return false;
    }

    return finish_open(tcp_socket, descriptor);
}

bool tcp_socket_listen(tcp_socket_t *tcp_socket, const char *listen_address, uint16_t port, int backlog) {
    if (!arguments_are_valid(tcp_socket, listen_address) || backlog <= 0) {
        errno = EINVAL;
        return false;
    }
    tcp_socket_init(tcp_socket);

    struct sockaddr_in address;
    if (!parse_address(listen_address, port, &address)) return false;

    const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1) return false;
    if (!enable_reuse_address(descriptor) || bind(descriptor, (const struct sockaddr *)&address, sizeof(address)) == -1 || listen(descriptor, backlog) == -1) {
        const int saved_errno = errno;
        close(descriptor);
        errno = saved_errno;
        return false;
    }

    return finish_open(tcp_socket, descriptor);
}

bool tcp_socket_accept(const tcp_socket_t *listener, tcp_socket_t *client) {
    if (listener == NULL || listener->descriptor == -1 || client == NULL) {
        errno = EINVAL;
        return false;
    }
    tcp_socket_init(client);

    int descriptor;
    do {
        descriptor = accept(listener->descriptor, NULL, NULL);
    } while (descriptor == -1 && errno == EINTR);

    if (descriptor == -1) return false;
    return finish_open(client, descriptor);
}

bool tcp_socket_set_nonblocking(tcp_socket_t *tcp_socket) {
    if (tcp_socket == NULL || tcp_socket->descriptor == -1) {
        errno = EINVAL;
        return false;
    }

    const int flags = fcntl(tcp_socket->descriptor, F_GETFL);
    if (flags == -1) return false;
    if ((flags & O_NONBLOCK) != 0) return true;
    return fcntl(tcp_socket->descriptor, F_SETFL, flags | O_NONBLOCK) != -1;
}

ssize_t tcp_socket_send(const tcp_socket_t *tcp_socket, const void *data, size_t size) {
    if (tcp_socket == NULL || tcp_socket->descriptor == -1 || data == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }

    ssize_t sent;
    do {
        sent = send(tcp_socket->descriptor, data, size, MSG_NOSIGNAL);
    } while (sent == -1 && errno == EINTR);
    return sent;
}

ssize_t tcp_socket_receive(const tcp_socket_t *tcp_socket, void *buffer, size_t capacity) {
    if (tcp_socket == NULL || tcp_socket->descriptor == -1 || buffer == NULL || capacity == 0) {
        errno = EINVAL;
        return -1;
    }

    ssize_t received;
    do {
        received = recv(tcp_socket->descriptor, buffer, capacity, 0);
    } while (received == -1 && errno == EINTR);
    return received;
}

void tcp_socket_close(tcp_socket_t *tcp_socket) {
    if (tcp_socket == NULL) return;
    if (tcp_socket->descriptor != -1) close(tcp_socket->descriptor);
    tcp_socket->descriptor = -1;
}
