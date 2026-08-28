#include "signal_handler.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t child_changed = 0;
static volatile sig_atomic_t notification_write_descriptor = -1;
static int notification_read_descriptor = -1;

static void notify_main_loop(void) {
    const int saved_errno = errno;
    const uint8_t byte = UINT8_C(1);
    if (notification_write_descriptor != -1) {
        const ssize_t ignored = write((int)notification_write_descriptor, &byte, sizeof(byte));
        (void)ignored;
    }
    errno = saved_errno;
}

static void handle_stop(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
    notify_main_loop();
}

static void handle_child(int signal_number) {
    (void)signal_number;
    child_changed = 1;
    notify_main_loop();
}

static bool set_descriptor_flags(int descriptor) {
    const int status_flags = fcntl(descriptor, F_GETFL);
    if (status_flags == -1 || fcntl(descriptor, F_SETFL, status_flags | O_NONBLOCK) == -1) return false;
    const int descriptor_flags = fcntl(descriptor, F_GETFD);
    return descriptor_flags != -1 && fcntl(descriptor, F_SETFD, descriptor_flags | FD_CLOEXEC) != -1;
}

static bool open_notification_pipe(void) {
    int descriptors[2];
    if (pipe(descriptors) == -1) return false;
    if (!set_descriptor_flags(descriptors[0]) || !set_descriptor_flags(descriptors[1])) {
        const int saved_errno = errno;
        close(descriptors[0]);
        close(descriptors[1]);
        errno = saved_errno;
        return false;
    }
    notification_read_descriptor = descriptors[0];
    notification_write_descriptor = descriptors[1];
    return true;
}

static bool set_handler(int signal_number, void (*handler)(int)) {
    struct sigaction action = {
        .sa_handler = handler,
        .sa_flags = 0
    };
    return sigemptyset(&action.sa_mask) == 0 && sigaction(signal_number, &action, NULL) == 0;
}

static bool set_default(int signal_number) {
    struct sigaction action = {
        .sa_handler = SIG_DFL,
        .sa_flags = 0
    };
    return sigemptyset(&action.sa_mask) == 0 && sigaction(signal_number, &action, NULL) == 0;
}

bool signal_handler_install(void) {
    signal_handler_close();
    stop_requested = 0;
    child_changed = 0;
    if (!open_notification_pipe()) return false;
    if (set_handler(SIGINT, handle_stop) &&
        set_handler(SIGTERM, handle_stop) &&
        set_handler(SIGCHLD, handle_child)) {
        return true;
    }
    const int saved_errno = errno;
    signal_handler_close();
    errno = saved_errno;
    return false;
}

bool signal_handler_restore_defaults(void) {
    stop_requested = 0;
    child_changed = 0;
    const bool restored = set_default(SIGINT) && set_default(SIGTERM) && set_default(SIGCHLD);
    signal_handler_close();
    return restored;
}

int signal_handler_descriptor(void) {
    return notification_read_descriptor;
}

void signal_handler_drain(void) {
    if (notification_read_descriptor == -1) return;
    uint8_t buffer[64];
    while (read(notification_read_descriptor, buffer, sizeof(buffer)) > 0) {
    }
}

void signal_handler_close(void) {
    const int write_descriptor = (int)notification_write_descriptor;
    notification_write_descriptor = -1;
    if (notification_read_descriptor != -1) close(notification_read_descriptor);
    if (write_descriptor != -1) close(write_descriptor);
    notification_read_descriptor = -1;
}

bool signal_handler_stop_requested(void) {
    return stop_requested != 0;
}

bool signal_handler_child_changed(void) {
    return child_changed != 0;
}

void signal_handler_clear_child_changed(void) {
    child_changed = 0;
}
