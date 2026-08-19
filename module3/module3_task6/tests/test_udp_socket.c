#include "udp_socket.h"

#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_TIMEOUT_MS 2000

int main(void) {
    const uint16_t port = (uint16_t)(40000U + (unsigned int)getpid() % 20000U);
    const char message[] = "udp broadcast test";
    char buffer[sizeof(message)] = {0};
    udp_socket_t sender;
    udp_socket_t receiver;

    if (!udp_socket_open(&sender, "127.255.255.255", port)) {
        perror("udp_socket_open sender");
        return 1;
    }

    if (!udp_socket_open(&receiver, "127.255.255.255", port)) {
        perror("udp_socket_open receiver");
        udp_socket_close(&sender);
        return 1;
    }

    if (udp_socket_send(&sender, message, sizeof(message)) != (ssize_t)sizeof(message)) {
        perror("udp_socket_send");
        udp_socket_close(&receiver);
        udp_socket_close(&sender);
        return 1;
    }

    struct pollfd event = {.fd = receiver.descriptor, .events = POLLIN};
    const int poll_status = poll(&event, 1, TEST_TIMEOUT_MS);

    if (poll_status != 1 || (event.revents & POLLIN) == 0) {
        fprintf(stderr, "Receiver did not get the broadcast datagram\n");
        udp_socket_close(&receiver);
        udp_socket_close(&sender);
        return 1;
    }

    const ssize_t received = udp_socket_receive(&receiver, buffer, sizeof(buffer), NULL);
    udp_socket_close(&receiver);
    udp_socket_close(&sender);

    if (received != (ssize_t)sizeof(message) || memcmp(buffer, message, sizeof(message)) != 0) {
        fprintf(stderr, "Received datagram does not match the sent datagram\n");
        return 1;
    }

    puts("UDP transport test passed");
    return 0;
}
