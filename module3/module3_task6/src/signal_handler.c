#include "signal_handler.h"

#include <stddef.h>

static volatile sig_atomic_t stop_requested = 0;

static void handle_sigint(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

int signal_handler_install(void) {
    struct sigaction action = {0};

    action.sa_handler = handle_sigint;
    action.sa_flags = 0;
    if (sigemptyset(&action.sa_mask) == -1) return -1;

    stop_requested = 0;
    return sigaction(SIGINT, &action, NULL);
}

sig_atomic_t signal_handler_stop_requested(void) {
    return stop_requested;
}
