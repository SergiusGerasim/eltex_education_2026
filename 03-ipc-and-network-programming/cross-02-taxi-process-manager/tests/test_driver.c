#include "driver.h"
#include "protocol.h"
#include "tcp_socket.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LOOPBACK_ADDRESS "127.0.0.1"

static bool wait_for_read(int descriptor, int seconds) {
    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(descriptor, &descriptors);
    struct timeval timeout = {.tv_sec = seconds};

    int status;
    do {
        status = select(descriptor + 1, &descriptors, NULL, NULL, &timeout);
    } while (status == -1 && errno == EINTR);
    return status > 0;
}

static void send_frame(const tcp_socket_t *socket, const taxi_message_t *message) {
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t size = 0;
    assert(protocol_encode(message, buffer, sizeof(buffer), &size));
    assert(tcp_socket_send(socket, buffer, size) == (ssize_t)size);
}

static taxi_message_t receive_frame(const tcp_socket_t *socket) {
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t offset = 0;
    while (offset < sizeof(buffer)) {
        assert(wait_for_read(socket->descriptor, 2));
        const ssize_t size = tcp_socket_receive(socket, buffer + offset, sizeof(buffer) - offset);
        assert(size > 0);
        offset += (size_t)size;
    }

    taxi_message_t message;
    assert(protocol_decode(buffer, sizeof(buffer), &message));
    return message;
}

static void test_driver_lifecycle(void) {
    tcp_socket_t listener;
    tcp_socket_t connection;
    tcp_socket_init(&listener);
    tcp_socket_init(&connection);
    assert(tcp_socket_listen(&listener, LOOPBACK_ADDRESS, 0, 4));

    uint16_t port = 0;
    assert(tcp_socket_get_local_port(&listener, &port));

    const pid_t child = fork();
    assert(child != -1);
    if (child == 0) {
        tcp_socket_close(&listener);
        const driver_config_t config = {
            .server_address = LOOPBACK_ADDRESS,
            .server_port = port
        };
        _exit(driver_run(&config));
    }

    assert(wait_for_read(listener.descriptor, 2));
    assert(tcp_socket_accept(&listener, &connection));

    taxi_message_t response = receive_frame(&connection);
    assert(response.type == TAXI_MESSAGE_HELLO);
    assert(response.driver_pid == (uint32_t)child);

    send_frame(&connection, &(taxi_message_t){
        .type = TAXI_MESSAGE_GET_STATUS,
        .request_id = 1
    });
    response = receive_frame(&connection);
    assert(response.type == TAXI_MESSAGE_STATUS_AVAILABLE);
    assert(response.request_id == 1);

    send_frame(&connection, &(taxi_message_t){
        .type = TAXI_MESSAGE_SEND_TASK,
        .request_id = 2,
        .task_timer = 1
    });
    response = receive_frame(&connection);
    assert(response.type == TAXI_MESSAGE_TASK_ACCEPTED);
    assert(response.task_timer == 1);

    send_frame(&connection, &(taxi_message_t){
        .type = TAXI_MESSAGE_SEND_TASK,
        .request_id = 3,
        .task_timer = 10
    });
    response = receive_frame(&connection);
    assert(response.type == TAXI_MESSAGE_STATUS_BUSY);
    assert(response.task_timer == 1);

    const struct timespec delay = {.tv_sec = 1, .tv_nsec = 100000000L};
    assert(nanosleep(&delay, NULL) == 0);

    send_frame(&connection, &(taxi_message_t){
        .type = TAXI_MESSAGE_GET_STATUS,
        .request_id = 4
    });
    response = receive_frame(&connection);
    assert(response.type == TAXI_MESSAGE_STATUS_AVAILABLE);

    send_frame(&connection, &(taxi_message_t){
        .type = TAXI_MESSAGE_SEND_TASK,
        .request_id = 5,
        .task_timer = 0
    });
    response = receive_frame(&connection);
    assert(response.type == TAXI_MESSAGE_ERROR);
    assert(response.task_timer == TAXI_ERROR_INVALID_TIMER);

    send_frame(&connection, &(taxi_message_t){.type = TAXI_MESSAGE_SHUTDOWN});

    int child_status = 0;
    assert(waitpid(child, &child_status, 0) == child);
    assert(WIFEXITED(child_status));
    assert(WEXITSTATUS(child_status) == EXIT_SUCCESS);

    tcp_socket_close(&connection);
    tcp_socket_close(&listener);
}

int main(void) {
    assert(driver_run(NULL) == EXIT_FAILURE);
    assert(errno == EINVAL);
    test_driver_lifecycle();
    puts("driver tests passed");
    return 0;
}
