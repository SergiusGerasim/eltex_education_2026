#include "signal_handler.h"

#include <signal.h>
#include <stdio.h>

int main(void) {
    if (!signal_handlers_install()) {
        fprintf(stderr, "Signal handler installation test failed\n");
        return 1;
    }

    if (signal_stop_requested()) {
        fprintf(stderr, "Unexpected initial stop request\n");
        signal_handlers_restore();
        return 1;
    }

    if (raise(SIGINT) != 0 || !signal_stop_requested()) {
        fprintf(stderr, "SIGINT handling test failed\n");
        signal_handlers_restore();
        return 1;
    }

    signal_handlers_restore();

    if (!signal_handlers_install() || raise(SIGTERM) != 0 || !signal_stop_requested()) {
        fprintf(stderr, "SIGTERM handling test failed\n");
        signal_handlers_restore();
        return 1;
    }

    signal_handlers_restore();
    puts("Signal handler tests passed");
    return 0;
}
