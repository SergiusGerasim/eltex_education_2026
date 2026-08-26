#pragma once

#include "protocol.h"

#include <stdbool.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

#define CLIENT_DEFAULT_PORT UINT16_C(40000)

typedef struct {
    int descriptor;
    struct in_addr local_ip;
    struct in_addr server_ip;
    uint16_t local_port;
    uint16_t server_port;
} client_t;

void client_init(client_t *client);
bool client_open(client_t *client,struct in_addr local_ip,uint16_t local_port,struct in_addr server_ip,uint16_t server_port);
bool client_send_request(client_t *client,const uint8_t *payload,size_t payload_size,protocol_message_t *response);
bool client_send_close(client_t *client);
void client_close(client_t *client);
