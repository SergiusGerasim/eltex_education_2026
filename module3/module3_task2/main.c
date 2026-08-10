#include "broker.h"
#include "publisher.h"
#include "subscriber.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s -b\n", program_name);
    fprintf(stderr, "  %s -p <topic>\n", program_name);
    fprintf(stderr, "  %s -s <topic> [topic ...]\n", program_name);
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "-b") == 0) return run_broker();
    if (argc == 3 && strcmp(argv[1], "-p") == 0) return run_publisher(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "-s") == 0) return run_subscriber(argc - 2, &argv[2]);

    print_usage(argv[0]);
    return EXIT_FAILURE;
}
