#include "client.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_SERVER_ADDRESS "127.0.0.1"
#define DEFAULT_PORT UINT16_C(5000)

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s <name> [server_address] [port]\n", program);
    fprintf(stderr, "Example: %s Alice 127.0.0.1 5000\n", program);
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
    if (argc < 2 || argc > 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const size_t name_size = strlen(argv[1]);
    if (name_size == 0 || name_size > CHAT_NAME_MAX) {
        fprintf(stderr, "Name must contain from 1 to %u bytes\n", CHAT_NAME_MAX);
        return EXIT_FAILURE;
    }

    client_config_t config = {
        .name = argv[1],
        .server_address = argc > 2 ? argv[2] : DEFAULT_SERVER_ADDRESS,
        .port = DEFAULT_PORT
    };

    if (!is_ipv4_address(config.server_address)) {
        fprintf(stderr, "Invalid IPv4 server address: %s\n", config.server_address);
        return EXIT_FAILURE;
    }
    if (argc > 3 && !parse_port(argv[3], &config.port)) {
        fprintf(stderr, "Port must be an integer from 1 to %u\n", UINT16_MAX);
        return EXIT_FAILURE;
    }

    return client_run(&config);
}
