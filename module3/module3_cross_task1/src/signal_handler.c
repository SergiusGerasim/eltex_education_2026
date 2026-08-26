#define _POSIX_C_SOURCE 200809L

#include "signal_handler.h"

#include <stddef.h>

static volatile sig_atomic_t stop_requested = 0;

static void handle_stop_signal(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

int signal_handler_install(void) {
    struct sigaction action = {0};
    action.sa_handler = handle_stop_signal;
    stop_requested = 0;

    if (sigemptyset(&action.sa_mask) == -1) return 0;
    if (sigaction(SIGINT, &action, NULL) == -1) return 0;
    if (sigaction(SIGTERM, &action, NULL) == -1) return 0;
    return 1;
}

sig_atomic_t signal_handler_stop_requested(void) {
    return stop_requested;
}
