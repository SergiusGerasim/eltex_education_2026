#include "server.h"
#include "signal_handler.h"

#include <stdio.h>

int main(void) {
    if (!signal_handler_install()) {
        perror("sigaction");
        return 1;
    }

    printf("Echo server listens on UDP port %u\n", (unsigned int)SERVER_DEFAULT_PORT);
    if (server_run(SERVER_DEFAULT_PORT) == -1) {
        perror("server_run");
        return 1;
    }
    return 0;
}
