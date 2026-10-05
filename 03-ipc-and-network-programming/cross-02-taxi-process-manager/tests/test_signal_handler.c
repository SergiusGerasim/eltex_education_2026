#include "signal_handler.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>

int main(void) {
    assert(signal_handler_install());
    assert(!signal_handler_stop_requested());
    assert(!signal_handler_child_changed());
    assert(signal_handler_descriptor() >= 0);

    assert(raise(SIGCHLD) == 0);
    assert(signal_handler_child_changed());
    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(signal_handler_descriptor(), &descriptors);
    struct timeval timeout = {0};
    assert(select(signal_handler_descriptor() + 1, &descriptors, NULL, NULL, &timeout) == 1);
    signal_handler_drain();
    signal_handler_clear_child_changed();
    assert(!signal_handler_child_changed());

    assert(raise(SIGINT) == 0);
    assert(signal_handler_stop_requested());

    assert(signal_handler_restore_defaults());
    assert(signal_handler_descriptor() == -1);
    assert(!signal_handler_stop_requested());
    assert(!signal_handler_child_changed());
    puts("signal handler tests passed");
    return 0;
}
