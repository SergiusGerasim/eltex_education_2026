#include "tcp_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static bool parse_address(const char *text, uint16_t port, struct sockaddr_in *address) {
    if (text == NULL || address == NULL) {
        errno = EINVAL;
        return false;
    }

    *address = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };

    const int status = inet_pton(AF_INET, text, &address->sin_addr);
    if (status == 1) return true;
    if (status == 0) errno = EINVAL;
    return false;
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
    if (tcp_socket != NULL) tcp_socket->descriptor = -1;
}

bool tcp_socket_connect(tcp_socket_t *tcp_socket, const char *server_address, uint16_t port) {
    if (tcp_socket == NULL || server_address == NULL || port == 0) {
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
    if (tcp_socket == NULL || listen_address == NULL || backlog <= 0) {
        errno = EINVAL;
        return false;
    }
    tcp_socket_init(tcp_socket);

    struct sockaddr_in address;
    if (!parse_address(listen_address, port, &address)) return false;

    const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1) return false;

    const int reuse_address = 1;
    if (setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address)) == -1 ||
        bind(descriptor, (const struct sockaddr *)&address, sizeof(address)) == -1 ||
        listen(descriptor, backlog) == -1) {
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

bool tcp_socket_get_local_port(const tcp_socket_t *tcp_socket, uint16_t *port) {
    if (tcp_socket == NULL || tcp_socket->descriptor == -1 || port == NULL) {
        errno = EINVAL;
        return false;
    }

    struct sockaddr_in address;
    socklen_t address_size = sizeof(address);
    if (getsockname(tcp_socket->descriptor, (struct sockaddr *)&address, &address_size) == -1) return false;
    if (address.sin_family != AF_INET) {
        errno = EAFNOSUPPORT;
        return false;
    }

    *port = ntohs(address.sin_port);
    return true;
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

    ssize_t sent_size;
    do {
        sent_size = send(tcp_socket->descriptor, data, size, MSG_NOSIGNAL);
    } while (sent_size == -1 && errno == EINTR);
    return sent_size;
}

ssize_t tcp_socket_receive(const tcp_socket_t *tcp_socket, void *buffer, size_t capacity) {
    if (tcp_socket == NULL || tcp_socket->descriptor == -1 || buffer == NULL || capacity == 0) {
        errno = EINVAL;
        return -1;
    }

    ssize_t received_size;
    do {
        received_size = recv(tcp_socket->descriptor, buffer, capacity, 0);
    } while (received_size == -1 && errno == EINTR);
    return received_size;
}

void tcp_socket_close(tcp_socket_t *tcp_socket) {
    if (tcp_socket == NULL) return;
    if (tcp_socket->descriptor != -1) close(tcp_socket->descriptor);
    tcp_socket->descriptor = -1;
}
