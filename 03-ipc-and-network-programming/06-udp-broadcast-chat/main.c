#include "chat.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BROADCAST_ADDRESS "255.255.255.255"
#define DEFAULT_PORT UINT16_C(5000)

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s <name> [broadcast_address] [port]\n", program);
    fprintf(stderr, "Example: %s Alice 192.168.1.255 5000\n", program);
}

static int parse_port(const char *text, uint16_t *port) {
    if (text == NULL || port == NULL || *text == '\0' || *text == '-') return 0;

    errno = 0;
    char *end = NULL;
    const unsigned long value = strtoul(text, &end, 10);

    if (errno != 0 || *end != '\0' || value == 0 || value > UINT16_MAX) return 0;

    *port = (uint16_t)value;
    return 1;
}

static int is_ipv4_address(const char *text) {
    struct in_addr address;

    return text != NULL && inet_pton(AF_INET, text, &address) == 1;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const size_t name_length = strlen(argv[1]);
    if (name_length == 0 || name_length > CHAT_NAME_MAX) {
        fprintf(stderr, "Name must contain from 1 to %u bytes\n", CHAT_NAME_MAX);
        return EXIT_FAILURE;
    }

    chat_config_t config = {
        .name = argv[1],
        .broadcast_address = argc > 2 ? argv[2] : DEFAULT_BROADCAST_ADDRESS,
        .port = DEFAULT_PORT
    };

    if (!is_ipv4_address(config.broadcast_address)) {
        fprintf(stderr, "Invalid IPv4 broadcast address: %s\n", config.broadcast_address);
        return EXIT_FAILURE;
    }

    if (argc > 3 && !parse_port(argv[3], &config.port)) {
        fprintf(stderr, "Port must be an integer from 1 to %u\n", UINT16_MAX);
        return EXIT_FAILURE;
    }

    return chat_run(&config);
}
