#include "udp_socket.h"

#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

static bool socket_option_enable(int descriptor, int option){
    const int enabled = 1;

    return setsockopt(descriptor, SOL_SOCKET, option, &enabled, sizeof(enabled)) != -1;
}

bool udp_socket_open(udp_socket_t *udp_socket, const char *broadcast_address, uint16_t port) {
    if (udp_socket == NULL) {
        errno = EINVAL;
        return false;
    }
    udp_socket->descriptor = -1;
    memset(&udp_socket->broadcast_address, 0, sizeof(udp_socket->broadcast_address));

    if (broadcast_address == NULL || port == 0) {
        errno = EINVAL;
        return false;
    }

    struct in_addr destination_address;
    const int parse_status = inet_pton(AF_INET, broadcast_address, &destination_address);
    if (parse_status != 1) {
        if (parse_status == 0) errno = EINVAL;
        return false;
    }

    const int temp_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (temp_descriptor == -1){
        return false;
    }
    if (
        !socket_option_enable(temp_descriptor, SO_BROADCAST) || 
        !socket_option_enable(temp_descriptor, SO_REUSEADDR)
    ) {
        const int saved_errno = errno;
        close(temp_descriptor);
        errno = saved_errno;
        return false;
    }
    struct sockaddr_in local_address = {0};
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(port);
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    // INADDR_ANY - принимать сообщения через любой сетевой интерфейс

    int bind_status = bind(temp_descriptor,
    (const struct sockaddr *)&local_address,
    sizeof(local_address)
    );
    // bind копирует содержимое local_address в состояние сокета внутри ядра Linux
    // т.е. локальный адресс 0.0.0.0:5000 будет хранится внутри ядра, а к сокету будем обращаться
    // через дескриптор. (при желании можно получить локальный адресс через систменый вызов getsockname())
    if (bind_status != 0){
        const int saved_errno = errno;
        close(temp_descriptor);
        errno = saved_errno;
        return false;
    }

    udp_socket->broadcast_address.sin_family = AF_INET;
    udp_socket->broadcast_address.sin_port = htons(port);
    udp_socket->broadcast_address.sin_addr = destination_address;
    // local_address используется для приема и bind
    // udp_socket->broadcast_address используется для отправки
    udp_socket->descriptor = temp_descriptor;

    return true;
}

void udp_socket_close(udp_socket_t *udp_socket) {
    if (udp_socket == NULL) return;

    if (udp_socket->descriptor != -1) {
        close(udp_socket->descriptor);
        udp_socket->descriptor = -1;
    }

    memset(&udp_socket->broadcast_address, 0, sizeof(udp_socket->broadcast_address));
}

ssize_t udp_socket_send(const udp_socket_t *udp_socket, const void *data, size_t size) {
    if (udp_socket == NULL || udp_socket->descriptor == -1 || data == NULL || size == 0 ) {
        errno = EINVAL;
        return -1;
    }
    ssize_t sent;
    do{
        sent = sendto(udp_socket->descriptor, data, size, 0, (const struct sockaddr *)&udp_socket->broadcast_address, sizeof(udp_socket->broadcast_address));
    } while (sent == -1 && errno == EINTR);
    
    return sent;
}

ssize_t udp_socket_receive(const udp_socket_t *udp_socket, void *buffer, size_t capacity, struct sockaddr_in *sender_address) {
    if (udp_socket == NULL || udp_socket->descriptor == -1 || buffer == NULL || capacity == 0){
        errno = EINVAL;
        return -1;
    }
    
    struct sockaddr_in discareded_address;

    struct sockaddr_in *actual_sender = sender_address != NULL ? sender_address : &discareded_address;

    socklen_t address_size = sizeof(*actual_sender);
    ssize_t received;
    do{
        received = recvfrom(udp_socket->descriptor, buffer, capacity, 0, (struct sockaddr *)actual_sender, &address_size);
    } while (received == -1 && errno == EINTR);

    return received;
}
