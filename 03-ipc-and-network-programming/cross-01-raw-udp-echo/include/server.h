#pragma once 

#include "protocol.h"

#include <stdbool.h>
#include <netinet/in.h>
#include <stdint.h>

#define SERVER_DEFAULT_PORT UINT16_C(5000)

typedef struct server_client server_client_t;

typedef struct {
    server_client_t *clients;
}server_state_t;


void server_state_init(server_state_t *state);
void server_state_destroy(server_state_t *state);

bool server_state_next_counter(server_state_t *state,struct in_addr client_ip,uint16_t client_port,uint32_t *counter);

bool server_state_remove_client(server_state_t *state,struct in_addr client_ip,uint16_t client_port);

bool server_handle_message(
    server_state_t *state,
    struct in_addr client_ip,
    uint16_t client_port,
    const protocol_message_t *request,
    protocol_message_t *response,
    bool *should_reply
);

int server_run(uint16_t server_port);
