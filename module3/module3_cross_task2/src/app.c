#include "app.h"

#include "cli.h"
#include "driver.h"
#include "protocol.h"
#include "signal_handler.h"
#include "tcp_socket.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define APP_ADDRESS "127.0.0.1"
#define APP_BACKLOG 16
#define APP_DRIVER_MAX 64U
#define APP_INPUT_CAPACITY (CLI_LINE_MAX * 2U + 2U)
#define APP_CONNECTION_INPUT_CAPACITY (TAXI_PROTOCOL_FRAME_SIZE * 4U)
#define APP_SHUTDOWN_GRACE_NANOSECONDS 500000000L
#define APP_SHUTDOWN_POLL_NANOSECONDS 10000000L

typedef struct {
    pid_t process_id;
    tcp_socket_t socket;
    uint8_t input[APP_CONNECTION_INPUT_CAPACITY];
    size_t input_size;
    bool connected;
} app_driver_t;

typedef struct {
    tcp_socket_t socket;
    uint8_t input[APP_CONNECTION_INPUT_CAPACITY];
    size_t input_size;
} pending_connection_t;

typedef struct {
    tcp_socket_t listener;
    uint16_t port;
    app_driver_t drivers[APP_DRIVER_MAX];
    size_t driver_count;
    pending_connection_t pending[APP_DRIVER_MAX];
    size_t pending_count;
    char cli_input[APP_INPUT_CAPACITY];
    size_t cli_input_size;
    uint32_t next_request_id;
} app_t;

static app_driver_t *find_driver(app_t *app, uint32_t process_id) {
    for (size_t index = 0; index < app->driver_count; ++index) {
        if ((uint32_t)app->drivers[index].process_id == process_id) return &app->drivers[index];
    }
    return NULL;
}

static uint32_t next_request_id(app_t *app) {
    ++app->next_request_id;
    if (app->next_request_id == 0) ++app->next_request_id;
    return app->next_request_id;
}

static bool wait_until_writable(int descriptor) {
    while (true) {
        fd_set descriptors;
        FD_ZERO(&descriptors);
        FD_SET(descriptor, &descriptors);
        const int status = select(descriptor + 1, NULL, &descriptors, NULL, NULL);
        if (status > 0) return true;
        if (status == -1 && errno == EINTR) continue;
        return false;
    }
}

static bool send_message(const tcp_socket_t *socket, const taxi_message_t *message) {
    uint8_t buffer[TAXI_PROTOCOL_FRAME_SIZE];
    size_t size = 0;
    if (!protocol_encode(message, buffer, sizeof(buffer), &size)) return false;
    size_t offset = 0;
    while (offset < size) {
        const ssize_t sent_size = tcp_socket_send(socket, buffer + offset, size - offset);
        if (sent_size > 0) {
            offset += (size_t)sent_size;
        } else if (sent_size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (!wait_until_writable(socket->descriptor)) return false;
        } else {
            return false;
        }
    }
    return true;
}

static void remove_pending(app_t *app, size_t index) {
    tcp_socket_close(&app->pending[index].socket);
    if (index + 1U < app->pending_count) app->pending[index] = app->pending[app->pending_count - 1U];
    --app->pending_count;
}

static void remove_driver(app_t *app, size_t index) {
    const pid_t process_id = app->drivers[index].process_id;
    tcp_socket_close(&app->drivers[index].socket);
    if (index + 1U < app->driver_count) app->drivers[index] = app->drivers[app->driver_count - 1U];
    --app->driver_count;
    printf("Driver disconnected: %ld\n", (long)process_id);
}

static void reap_children(app_t *app) {
    int status = 0;
    pid_t process_id;
    while ((process_id = waitpid(-1, &status, WNOHANG)) > 0) {
        for (size_t index = 0; index < app->driver_count; ++index) {
            if (app->drivers[index].process_id == process_id) {
                remove_driver(app, index);
                break;
            }
        }
    }
}

static void close_inherited_descriptors(app_t *app) {
    tcp_socket_close(&app->listener);
    for (size_t index = 0; index < app->driver_count; ++index) tcp_socket_close(&app->drivers[index].socket);
    for (size_t index = 0; index < app->pending_count; ++index) tcp_socket_close(&app->pending[index].socket);
}

static void create_driver(app_t *app) {
    if (app->driver_count == APP_DRIVER_MAX) {
        fprintf(stderr, "Driver limit reached\n");
        return;
    }
    const pid_t process_id = fork();
    if (process_id == -1) {
        perror("fork");
        return;
    }
    if (process_id == 0) {
        close_inherited_descriptors(app);
        if (!signal_handler_restore_defaults()) _exit(EXIT_FAILURE);
        const driver_config_t config = {.server_address = APP_ADDRESS, .server_port = app->port};
        _exit(driver_run(&config));
    }
    app_driver_t *driver = &app->drivers[app->driver_count++];
    *driver = (app_driver_t){.process_id = process_id};
    tcp_socket_init(&driver->socket);
    printf("Driver created: %ld\n", (long)process_id);
}

static bool send_driver_request(app_t *app, app_driver_t *driver, taxi_message_type_t type, uint32_t timer) {
    if (!driver->connected) {
        fprintf(stderr, "Driver %ld is not connected yet\n", (long)driver->process_id);
        return false;
    }
    const taxi_message_t message = {
        .type = type,
        .request_id = next_request_id(app),
        .driver_pid = (uint32_t)driver->process_id,
        .task_timer = timer
    };
    if (send_message(&driver->socket, &message)) return true;
    perror("send driver request");
    return false;
}

static void execute_command(app_t *app, const cli_command_t *command) {
    if (command->type == CLI_COMMAND_CREATE_DRIVER) {
        create_driver(app);
        return;
    }
    if (command->type == CLI_COMMAND_GET_DRIVERS) {
        if (app->driver_count == 0) {
            puts("No drivers");
            return;
        }
        for (size_t index = 0; index < app->driver_count; ++index) {
            send_driver_request(app, &app->drivers[index], TAXI_MESSAGE_GET_STATUS, 0);
        }
        return;
    }
    app_driver_t *driver = find_driver(app, command->driver_pid);
    if (driver == NULL) {
        fprintf(stderr, "Unknown driver: %u\n", command->driver_pid);
        return;
    }
    if (command->type == CLI_COMMAND_SEND_TASK) {
        send_driver_request(app, driver, TAXI_MESSAGE_SEND_TASK, command->task_timer);
    } else if (command->type == CLI_COMMAND_GET_STATUS) {
        send_driver_request(app, driver, TAXI_MESSAGE_GET_STATUS, 0);
    }
}

static void process_cli_line(app_t *app, const char *line) {
    cli_command_t command;
    const cli_parse_result_t result = cli_parse_command(line, &command);
    if (result == CLI_PARSE_COMPLETE) execute_command(app, &command);
    else if (result == CLI_PARSE_INVALID) fputs(cli_usage(), stderr);
}

static void process_cli_buffer(app_t *app) {
    size_t line_start = 0;
    for (size_t index = 0; index < app->cli_input_size; ++index) {
        if (app->cli_input[index] != '\n') continue;
        const size_t line_size = index - line_start;
        if (line_size <= CLI_LINE_MAX) {
            char line[CLI_LINE_MAX + 1U];
            memcpy(line, app->cli_input + line_start, line_size);
            line[line_size] = '\0';
            process_cli_line(app, line);
        } else {
            fputs(cli_usage(), stderr);
        }
        line_start = index + 1U;
    }
    if (line_start != 0) {
        memmove(app->cli_input, app->cli_input + line_start, app->cli_input_size - line_start);
        app->cli_input_size -= line_start;
    }
}

static bool receive_cli_input(app_t *app, bool *running) {
    if (app->cli_input_size == sizeof(app->cli_input)) {
        app->cli_input_size = 0;
        fputs(cli_usage(), stderr);
    }
    const ssize_t size = read(STDIN_FILENO, app->cli_input + app->cli_input_size,
                              sizeof(app->cli_input) - app->cli_input_size);
    if (size > 0) {
        app->cli_input_size += (size_t)size;
        process_cli_buffer(app);
        return true;
    }
    if (size == 0) {
        if (app->cli_input_size != 0) {
            app->cli_input[app->cli_input_size] = '\0';
            process_cli_line(app, app->cli_input);
        }
        *running = false;
        return true;
    }
    return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK;
}

static void accept_connections(app_t *app) {
    while (app->pending_count < APP_DRIVER_MAX) {
        pending_connection_t *connection = &app->pending[app->pending_count];
        tcp_socket_init(&connection->socket);
        if (!tcp_socket_accept(&app->listener, &connection->socket)) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) perror("accept");
            return;
        }
        connection->input_size = 0;
        ++app->pending_count;
    }
}

static bool register_connection(app_t *app, size_t pending_index, const taxi_message_t *hello) {
    app_driver_t *driver = find_driver(app, hello->driver_pid);
    if (driver == NULL || driver->connected || hello->type != TAXI_MESSAGE_HELLO) return false;
    pending_connection_t *pending = &app->pending[pending_index];
    driver->socket = pending->socket;
    driver->connected = true;
    driver->input_size = pending->input_size;
    memcpy(driver->input, pending->input, pending->input_size);
    tcp_socket_init(&pending->socket);
    remove_pending(app, pending_index);
    printf("Driver connected: %ld\n", (long)driver->process_id);
    return true;
}

static bool receive_pending(app_t *app, size_t index) {
    pending_connection_t *connection = &app->pending[index];
    const ssize_t size = tcp_socket_receive(&connection->socket, connection->input + connection->input_size,
                                            sizeof(connection->input) - connection->input_size);
    if (size <= 0) return size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);
    connection->input_size += (size_t)size;
    size_t frame_size = 0;
    const protocol_frame_status_t status = protocol_frame_size(connection->input, connection->input_size, &frame_size);
    if (status == PROTOCOL_FRAME_INCOMPLETE) return true;
    if (status == PROTOCOL_FRAME_INVALID) return false;
    taxi_message_t hello;
    if (!protocol_decode(connection->input, frame_size, &hello)) return false;
    memmove(connection->input, connection->input + frame_size, connection->input_size - frame_size);
    connection->input_size -= frame_size;
    return register_connection(app, index, &hello);
}

static void print_driver_response(const taxi_message_t *message) {
    switch (message->type) {
        case TAXI_MESSAGE_TASK_ACCEPTED:
            printf("Driver %u: Task accepted %u\n", message->driver_pid, message->task_timer);
            break;
        case TAXI_MESSAGE_STATUS_AVAILABLE:
            printf("Driver %u: Available\n", message->driver_pid);
            break;
        case TAXI_MESSAGE_STATUS_BUSY:
            printf("Driver %u: Busy %u\n", message->driver_pid, message->task_timer);
            break;
        case TAXI_MESSAGE_ERROR:
            printf("Driver %u: Error %u\n", message->driver_pid, message->task_timer);
            break;
        default:
            break;
    }
}

static bool process_driver_input(app_driver_t *driver) {
    while (driver->input_size != 0) {
        size_t frame_size = 0;
        const protocol_frame_status_t status = protocol_frame_size(driver->input, driver->input_size, &frame_size);
        if (status == PROTOCOL_FRAME_INCOMPLETE) return true;
        if (status == PROTOCOL_FRAME_INVALID) return false;
        taxi_message_t message;
        if (!protocol_decode(driver->input, frame_size, &message) ||
            message.driver_pid != (uint32_t)driver->process_id) return false;
        print_driver_response(&message);
        memmove(driver->input, driver->input + frame_size, driver->input_size - frame_size);
        driver->input_size -= frame_size;
    }
    return true;
}

static bool receive_driver(app_driver_t *driver) {
    if (driver->input_size == sizeof(driver->input)) return false;
    const ssize_t size = tcp_socket_receive(&driver->socket, driver->input + driver->input_size,
                                            sizeof(driver->input) - driver->input_size);
    if (size > 0) {
        driver->input_size += (size_t)size;
        return process_driver_input(driver);
    }
    return size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK);
}

static void build_read_set(const app_t *app, fd_set *descriptors, int *maximum) {
    FD_ZERO(descriptors);
    FD_SET(STDIN_FILENO, descriptors);
    FD_SET(app->listener.descriptor, descriptors);
    *maximum = app->listener.descriptor > STDIN_FILENO ? app->listener.descriptor : STDIN_FILENO;
    const int signal_descriptor = signal_handler_descriptor();
    FD_SET(signal_descriptor, descriptors);
    if (signal_descriptor > *maximum) *maximum = signal_descriptor;
    for (size_t index = 0; index < app->pending_count; ++index) {
        const int descriptor = app->pending[index].socket.descriptor;
        FD_SET(descriptor, descriptors);
        if (descriptor > *maximum) *maximum = descriptor;
    }
    for (size_t index = 0; index < app->driver_count; ++index) {
        if (!app->drivers[index].connected) continue;
        const int descriptor = app->drivers[index].socket.descriptor;
        FD_SET(descriptor, descriptors);
        if (descriptor > *maximum) *maximum = descriptor;
    }
}

static bool handle_ready_descriptors(app_t *app, fd_set *descriptors, bool *running) {
    if (FD_ISSET(STDIN_FILENO, descriptors) && !receive_cli_input(app, running)) return false;
    if (!*running) return true;
    if (FD_ISSET(app->listener.descriptor, descriptors)) accept_connections(app);
    for (size_t index = 0; index < app->pending_count;) {
        const int descriptor = app->pending[index].socket.descriptor;
        if (FD_ISSET(descriptor, descriptors) && !receive_pending(app, index)) remove_pending(app, index);
        else if (index < app->pending_count && app->pending[index].socket.descriptor == -1) continue;
        else ++index;
    }
    for (size_t index = 0; index < app->driver_count;) {
        if (app->drivers[index].connected && FD_ISSET(app->drivers[index].socket.descriptor, descriptors) &&
            !receive_driver(&app->drivers[index])) remove_driver(app, index);
        else ++index;
    }
    return true;
}

static int run_event_loop(app_t *app) {
    bool running = true;
    while (running) {
        signal_handler_drain();
        if (signal_handler_child_changed()) {
            signal_handler_clear_child_changed();
            reap_children(app);
        }
        if (signal_handler_stop_requested()) break;
        fd_set read_descriptors;
        int maximum_descriptor = 0;
        build_read_set(app, &read_descriptors, &maximum_descriptor);
        const int status = select(maximum_descriptor + 1, &read_descriptors, NULL, NULL, NULL);
        if (status == -1) {
            if (errno == EINTR) continue;
            perror("select");
            return EXIT_FAILURE;
        }
        if (FD_ISSET(signal_handler_descriptor(), &read_descriptors)) {
            signal_handler_drain();
            continue;
        }
        if (!handle_ready_descriptors(app, &read_descriptors, &running)) return EXIT_FAILURE;
        fflush(stdout);
        fflush(stderr);
    }
    return EXIT_SUCCESS;
}

static bool reap_driver_process(app_driver_t *driver) {
    if (driver->process_id <= 0) return true;
    int status = 0;
    const pid_t result = waitpid(driver->process_id, &status, WNOHANG);
    if (result == driver->process_id || (result == -1 && errno == ECHILD)) {
        driver->process_id = 0;
        return true;
    }
    return false;
}

static bool shutdown_deadline_reached(const struct timespec *deadline) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) return true;
    return now.tv_sec > deadline->tv_sec ||
           (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec);
}

static void shutdown_app(app_t *app) {
    const taxi_message_t shutdown_message = {.type = TAXI_MESSAGE_SHUTDOWN};
    for (size_t index = 0; index < app->driver_count; ++index) {
        if (app->drivers[index].connected) send_message(&app->drivers[index].socket, &shutdown_message);
        tcp_socket_close(&app->drivers[index].socket);
    }
    for (size_t index = 0; index < app->pending_count; ++index) {
        send_message(&app->pending[index].socket, &shutdown_message);
        tcp_socket_close(&app->pending[index].socket);
    }
    tcp_socket_close(&app->listener);

    struct timespec deadline;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) == 0) {
        deadline.tv_nsec += APP_SHUTDOWN_GRACE_NANOSECONDS;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }

        while (!shutdown_deadline_reached(&deadline)) {
            bool all_finished = true;
            for (size_t index = 0; index < app->driver_count; ++index) {
                if (!reap_driver_process(&app->drivers[index])) all_finished = false;
            }
            if (all_finished) return;
            const struct timespec pause = {.tv_nsec = APP_SHUTDOWN_POLL_NANOSECONDS};
            while (nanosleep(&pause, NULL) == -1 && errno == EINTR) {
            }
        }
    }

    for (size_t index = 0; index < app->driver_count; ++index) {
        if (app->drivers[index].process_id <= 0) continue;
        kill(app->drivers[index].process_id, SIGTERM);
        int status = 0;
        pid_t result;
        do {
            result = waitpid(app->drivers[index].process_id, &status, 0);
        } while (result == -1 && errno == EINTR);
    }
}

int app_run(void) {
    app_t app = {0};
    tcp_socket_init(&app.listener);
    if (!signal_handler_install()) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    if (!tcp_socket_listen(&app.listener, APP_ADDRESS, 0, APP_BACKLOG) ||
        !tcp_socket_get_local_port(&app.listener, &app.port)) {
        perror("listen");
        tcp_socket_close(&app.listener);
        signal_handler_close();
        return EXIT_FAILURE;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    const int result = run_event_loop(&app);
    shutdown_app(&app);
    signal_handler_close();
    return result;
}
