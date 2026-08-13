#include "chat.h"

#include <stdio.h>
#include <stdlib.h>

static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s <queue_name>\n", program_name);
    fprintf(stderr, "Example: %s /chat\n", program_name);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    return chat_run(argv[1]);
}
