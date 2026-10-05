#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_LISTEN_ADDRESS "0.0.0.0"
#define DEFAULT_PORT UINT16_C(5000)

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s [listen_address] [port]\n", program);
    fprintf(stderr, "Example: %s 0.0.0.0 5000\n", program);
}

static bool parse_port(const char *text, uint16_t *port) {
    if (text == NULL || port == NULL || *text == '\0' || *text == '-') return false;

    errno = 0;
    char *end = NULL;
    const unsigned long value = strtoul(text, &end, 10);

    if (errno != 0 || *end != '\0' || value == 0 || value > UINT16_MAX) return false;
    *port = (uint16_t)value;
    return true;
}

static bool is_ipv4_address(const char *text) {
    struct in_addr address;
    return text != NULL && inet_pton(AF_INET, text, &address) == 1;
}

int main(int argc, char **argv) {
    if (argc > 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    server_config_t config = {
        .listen_address = argc > 1 ? argv[1] : DEFAULT_LISTEN_ADDRESS,
        .port = DEFAULT_PORT
    };

    if (!is_ipv4_address(config.listen_address)) {
        fprintf(stderr, "Invalid IPv4 listen address: %s\n", config.listen_address);
        return EXIT_FAILURE;
    }
    if (argc > 2 && !parse_port(argv[2], &config.port)) {
        fprintf(stderr, "Port must be an integer from 1 to %u\n", UINT16_MAX);
        return EXIT_FAILURE;
    }

    return server_run(&config);
}
