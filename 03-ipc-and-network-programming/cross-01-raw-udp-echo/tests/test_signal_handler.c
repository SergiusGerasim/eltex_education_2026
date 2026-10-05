#include "signal_handler.h"

#include <signal.h>
#include <stdio.h>

static int test_signal(int signal_number) {
    if (!signal_handler_install()) return 0;
    if (signal_handler_stop_requested()) return 0;
    if (raise(signal_number) != 0) return 0;
    return signal_handler_stop_requested() != 0;
}

int main(void) {
    if (!test_signal(SIGINT)) {
        fprintf(stderr, "SIGINT handling test failed\n");
        return 1;
    }
    if (!test_signal(SIGTERM)) {
        fprintf(stderr, "SIGTERM handling test failed\n");
        return 1;
    }

    puts("Signal handler tests passed");
    return 0;
}
