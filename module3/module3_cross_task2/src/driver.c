#include "driver.h"

#include "protocol.h"
#include "tcp_socket.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define DRIVER_INPUT_CAPACITY (TAXI_PROTOCOL_FRAME_SIZE * 4U)

typedef struct {
    tcp_socket_t socket;
    uint8_t input[DRIVER_INPUT_CAPACITY];
    size_t input_size;
    bool busy;
    struct timespec deadline;
} driver_t;

static bool monotonic_now(struct timespec *time_value) {
    return clock_gettime(CLOCK_MONOTONIC, time_value) == 0;
}

static int compare_time(const struct timespec *left, const struct timespec *right) {
    if (left->tv_sec != right->tv_sec) return left->tv_sec < right->tv_sec ? -1 : 1;
    if (left->tv_nsec != right->tv_nsec) return left->tv_nsec < right->tv_nsec ? -1 : 1;
    return 0;
}

static void update_state(driver_t *driver, const struct timespec *now) {
    if (driver->busy && compare_time(now, &driver->deadline) >= 0) driver->busy = false;
}

static uint32_t remaining_seconds(const driver_t *driver, const struct timespec *now) {
    time_t seconds = driver->deadline.tv_sec - now->tv_sec;
    long nanoseconds = driver->deadline.tv_nsec - now->tv_nsec;
    if (nanoseconds > 0) {
        ++seconds;
    } else if (nanoseconds < 0) {
        --seconds;
        ++seconds;
    }

    if (seconds <= 0) return 0;
    if ((uintmax_t)seconds > UINT32_MAX) return UINT32_MAX;
    return (uint32_t)seconds;
}

static void make_timeout(const driver_t *driver, const struct timespec *now, struct timeval *timeout) {
    time_t seconds = driver->deadline.tv_sec - now->tv_sec;
    long nanoseconds = driver->deadline.tv_nsec - now->tv_nsec;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000L;
    }
    if (seconds < 0) {
        seconds = 0;
        nanoseconds = 0;
    }

    timeout->tv_sec = seconds;
    timeout->tv_usec = nanoseconds / 1000L;
}

static bool wait_until_writable(int descriptor) {
    while (true) {
        fd_set write_descriptors;
        FD_ZERO(&write_descriptors);
        FD_SET(descriptor, &write_descriptors);

        const int status = select(descriptor + 1, NULL, &write_descriptors, NULL, NULL);
        if (status > 0) return true;
        if (status == -1 && errno == EINTR) continue;
        return false;
    }
}

static bool send_message(const tcp_socket_t *socket, const taxi_message_t *message) {
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t encoded_size = 0;
    if (!protocol_encode(message, buffer, sizeof(buffer), &encoded_size)) return false;

    size_t offset = 0;
    while (offset < encoded_size) {
        const ssize_t sent_size = tcp_socket_send(socket, buffer + offset, encoded_size - offset);
        if (sent_size > 0) {
            offset += (size_t)sent_size;
            continue;
        }
        if (sent_size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (!wait_until_writable(socket->descriptor)) return false;
            continue;
        }
        return false;
    }
    return true;
}

static bool send_status(driver_t *driver, uint32_t request_id, const struct timespec *now) {
    const taxi_message_t response = {
        .type = driver->busy ? TAXI_MESSAGE_STATUS_BUSY : TAXI_MESSAGE_STATUS_AVAILABLE,
        .request_id = request_id,
        .driver_pid = (uint32_t)getpid(),
        .task_timer = driver->busy ? remaining_seconds(driver, now) : 0
    };
    return send_message(&driver->socket, &response);
}

static bool handle_message(driver_t *driver, const taxi_message_t *message, bool *running) {
    struct timespec now;
    if (!monotonic_now(&now)) return false;
    update_state(driver, &now);

    taxi_message_t response = {
        .request_id = message->request_id,
        .driver_pid = (uint32_t)getpid()
    };

    switch (message->type) {
        case TAXI_MESSAGE_SEND_TASK:
            if (driver->busy) return send_status(driver, message->request_id, &now);
            if (message->task_timer == 0) {
                response.type = TAXI_MESSAGE_ERROR;
                response.task_timer = TAXI_ERROR_INVALID_TIMER;
                return send_message(&driver->socket, &response);
            }
            driver->busy = true;
            driver->deadline = now;
            driver->deadline.tv_sec += (time_t)message->task_timer;
            response.type = TAXI_MESSAGE_TASK_ACCEPTED;
            response.task_timer = message->task_timer;
            return send_message(&driver->socket, &response);
        case TAXI_MESSAGE_GET_STATUS:
            return send_status(driver, message->request_id, &now);
        case TAXI_MESSAGE_SHUTDOWN:
            *running = false;
            return true;
        default:
            response.type = TAXI_MESSAGE_ERROR;
            response.task_timer = TAXI_ERROR_INVALID_MESSAGE;
            return send_message(&driver->socket, &response);
    }
}

static bool process_input(driver_t *driver, bool *running) {
    while (driver->input_size != 0) {
        size_t frame_size = 0;
        const protocol_frame_status_t status = protocol_frame_size(driver->input, driver->input_size, &frame_size);
        if (status == PROTOCOL_FRAME_INCOMPLETE) return true;
        if (status == PROTOCOL_FRAME_INVALID) return false;

        taxi_message_t message;
        if (!protocol_decode(driver->input, frame_size, &message) || !handle_message(driver, &message, running)) {
            return false;
        }

        memmove(driver->input, driver->input + frame_size, driver->input_size - frame_size);
        driver->input_size -= frame_size;
        if (!*running) return true;
    }
    return true;
}

static bool receive_input(driver_t *driver, bool *running) {
    if (driver->input_size == sizeof(driver->input)) return false;

    const ssize_t received_size = tcp_socket_receive(
        &driver->socket,
        driver->input + driver->input_size,
        sizeof(driver->input) - driver->input_size
    );
    if (received_size > 0) {
        driver->input_size += (size_t)received_size;
        return process_input(driver, running);
    }
    if (received_size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
    return false;
}

static bool send_hello(driver_t *driver) {
    const pid_t process_id = getpid();
    if (process_id <= 0 || (uintmax_t)process_id > UINT32_MAX) return false;

    const taxi_message_t hello = {
        .type = TAXI_MESSAGE_HELLO,
        .driver_pid = (uint32_t)process_id
    };
    return send_message(&driver->socket, &hello);
}

static int run_event_loop(driver_t *driver) {
    bool running = true;
    while (running) {
        struct timespec now;
        if (!monotonic_now(&now)) return EXIT_FAILURE;
        update_state(driver, &now);

        struct timeval timeout;
        struct timeval *timeout_pointer = NULL;
        if (driver->busy) {
            make_timeout(driver, &now, &timeout);
            timeout_pointer = &timeout;
        }

        fd_set read_descriptors;
        FD_ZERO(&read_descriptors);
        FD_SET(driver->socket.descriptor, &read_descriptors);

        const int status = select(driver->socket.descriptor + 1, &read_descriptors, NULL, NULL, timeout_pointer);
        if (status == -1) {
            if (errno == EINTR) continue;
            return EXIT_FAILURE;
        }
        if (status == 0) continue;
        if (FD_ISSET(driver->socket.descriptor, &read_descriptors) && !receive_input(driver, &running)) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int driver_run(const driver_config_t *config) {
    if (config == NULL || config->server_address == NULL || config->server_port == 0) {
        errno = EINVAL;
        return EXIT_FAILURE;
    }

    driver_t driver = {0};
    tcp_socket_init(&driver.socket);
    if (!tcp_socket_connect(&driver.socket, config->server_address, config->server_port)) {
        perror("driver connect");
        return EXIT_FAILURE;
    }

    int result = EXIT_FAILURE;
    if (send_hello(&driver)) result = run_event_loop(&driver);
    tcp_socket_close(&driver.socket);
    return result;
}
