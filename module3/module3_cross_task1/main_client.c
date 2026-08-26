#include "client.h"
#include "server.h"
#include "signal_handler.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program) {
    fprintf(stderr,"Usage: %s <local-ip> <server-ip> [client-port]\n",program);
}

static bool parse_port(const char *text,uint16_t *port) {
    if (text == NULL || port == NULL || text[0] == '\0' || text[0] == '-') return false;

    char *end = NULL;
    errno = 0;
    const unsigned long value = strtoul(text,&end,10);
    if (errno != 0 || *end != '\0' || value == 0UL || value > UINT16_MAX) return false;

    *port = (uint16_t)value;
    return true;
}

static void discard_line(void) {
    int character;
    do {
        character = getchar();
    } while (character != '\n' && character != EOF);
}

int main(int argc,char **argv) {
    if (argc != 3 && argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    struct in_addr local_ip;
    struct in_addr server_ip;
    uint16_t client_port = CLIENT_DEFAULT_PORT;
    if (inet_pton(AF_INET,argv[1],&local_ip) != 1) {
        fprintf(stderr,"Invalid local IPv4 address: %s\n",argv[1]);
        return 1;
    }
    if (inet_pton(AF_INET,argv[2],&server_ip) != 1) {
        fprintf(stderr,"Invalid server IPv4 address: %s\n",argv[2]);
        return 1;
    }
    if (argc == 4 && !parse_port(argv[3],&client_port)) {
        fprintf(stderr,"Invalid client port: %s\n",argv[3]);
        return 1;
    }
    if (!signal_handler_install()) {
        perror("sigaction");
        return 1;
    }

    client_t client;
    client_init(&client);
    if (!client_open(&client,local_ip,client_port,server_ip,SERVER_DEFAULT_PORT)) {
        perror("client_open");
        return 1;
    }

    char input[PROTOCOL_PAYLOAD_MAX + 2U];
    int result = 0;
    printf("Echo client uses UDP port %u. Enter /quit to exit.\n",(unsigned int)client_port);

    while (!signal_handler_stop_requested()) {
        fputs("> ",stdout);
        fflush(stdout);

        errno = 0;
        if (fgets(input,(int)sizeof(input),stdin) == NULL) {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            if (ferror(stdin)) {
                perror("fgets");
                result = 1;
            }
            break;
        }

        size_t input_size = strlen(input);
        if (input_size > 0U && input[input_size - 1U] == '\n') input[--input_size] = '\0';
        else if (!feof(stdin)) {
            discard_line();
            fprintf(stderr,"Message is longer than %u bytes\n",(unsigned int)PROTOCOL_PAYLOAD_MAX);
            continue;
        }
        if (input_size > PROTOCOL_PAYLOAD_MAX) {
            fprintf(stderr,"Message is longer than %u bytes\n",(unsigned int)PROTOCOL_PAYLOAD_MAX);
            continue;
        }
        if (strcmp(input,"/quit") == 0) break;

        protocol_message_t response;
        if (!client_send_request(&client,(const uint8_t *)input,input_size,&response)) {
            if (errno == EINTR && signal_handler_stop_requested()) break;
            perror("client_send_request");
            result = 1;
            break;
        }

        if (response.payload_size > 0U) fwrite(response.payload,1,response.payload_size,stdout);
        printf(" %u\n",(unsigned int)response.counter);
    }

    if (!client_send_close(&client)) {
        perror("client_send_close");
        result = 1;
    }
    client_close(&client);
    return result;
}
