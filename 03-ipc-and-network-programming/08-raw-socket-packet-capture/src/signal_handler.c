#include "signal_handler.h"

#include <signal.h>
#include <stddef.h>

static volatile sig_atomic_t stop_requested = 0;
static struct sigaction previous_sigint;
static struct sigaction previous_sigterm;
static int handlers_installed = 0;

static void handle_stop_signal(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

int signal_handlers_install(void) {
    struct sigaction action = {0};

    action.sa_handler = handle_stop_signal;
    if (sigemptyset(&action.sa_mask) == -1) return 0;

    stop_requested = 0;
    if (sigaction(SIGINT, &action, &previous_sigint) == -1) return 0;
    if (sigaction(SIGTERM, &action, &previous_sigterm) == -1) {
        sigaction(SIGINT, &previous_sigint, NULL);
        return 0;
    }

    handlers_installed = 1;
    return 1;
}

void signal_handlers_restore(void) {
    if (!handlers_installed) return;

    sigaction(SIGINT, &previous_sigint, NULL);
    sigaction(SIGTERM, &previous_sigterm, NULL);
    handlers_installed = 0;
}

int signal_stop_requested(void) {
    return stop_requested != 0;
}
