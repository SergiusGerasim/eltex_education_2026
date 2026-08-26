#include "server.h"
#include "raw_udp.h"
#include "signal_handler.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

struct server_client {
    struct in_addr ip;
    uint16_t port;
    uint32_t counter;
    struct server_client *next;
};

static bool client_matches(
    const server_client_t *client,
    struct in_addr ip,
    uint16_t port
) {
    return client->ip.s_addr == ip.s_addr &&
           client->port == port;
}

void server_state_init(server_state_t *state) {
    if (state == NULL) return;

    state->clients = NULL;
}

void server_state_destroy(server_state_t *state) {
    if (state == NULL) return;

    server_client_t *client = state->clients;

    while (client != NULL) {
        server_client_t *next = client->next;
        free(client);
        client = next;
    }

    state->clients = NULL;
}

bool server_state_next_counter(server_state_t *state,struct in_addr client_ip,uint16_t client_port,uint32_t *counter) {
    if (state == NULL || counter == NULL) return false;
    *counter = 0;

    server_client_t *client = state->clients;

    while (client != NULL) {
        if (client_matches(client, client_ip, client_port)) {
            if (client->counter == UINT32_MAX) return false;

            client->counter++;
            *counter = client->counter;
            return true;
        }

        client = client->next;
    }

    client = malloc(sizeof(*client));
    if (client == NULL) return false;

    client->ip = client_ip;
    client->port = client_port;
    client->counter = 1U;
    client->next = state->clients;

    state->clients = client;
    *counter = client->counter;

    return true;
}

bool server_state_remove_client(server_state_t *state,struct in_addr client_ip,uint16_t client_port) {
    if (state == NULL) return false;

    server_client_t **link = &state->clients;

    while (*link != NULL) {
        server_client_t *client = *link;

        if (client_matches(client, client_ip, client_port)) {
            *link = client->next;
            free(client);
            return true;
        }

        link = &client->next;
    }

    return false;
}

bool server_handle_message(server_state_t *state,struct in_addr client_ip,uint16_t client_port,
    const protocol_message_t *request,protocol_message_t *response,bool *should_reply) {
    if (should_reply == NULL) return false;
    *should_reply = false;

    if (state == NULL || request == NULL || response == NULL) return false;

    memset(response, 0, sizeof(*response));

    if (request->type == ECHO_CLOSE) {
        if (request->counter != 0U || request->payload_size != 0U) return false;

        (void)server_state_remove_client(state,client_ip,client_port);

        return true;
    }

    if (request->type != ECHO_REQUEST) return false;
    if (request->counter != 0U) return false;
    if (request->payload_size > PROTOCOL_PAYLOAD_MAX) return false;

    uint32_t counter;

    if (!server_state_next_counter(state, client_ip, client_port, &counter)) return false;

    response->type = ECHO_RESPONSE;
    response->counter = counter;
    response->payload_size = request->payload_size;

    if (request->payload_size > 0U) 
        memcpy(response->payload,request->payload,request->payload_size);
    

    response->payload[response->payload_size] = '\0';
    *should_reply = true;

    return true;
}

int server_run(uint16_t server_port) {
    const int descriptor = raw_udp_open();
    if (descriptor < 0) return -1;

    server_state_t state;
    server_state_init(&state);

    uint8_t received_packet[UINT16_MAX];
    uint8_t encoded_response[PROTOCOL_PACKET_MAX_SIZE];
    uint8_t outgoing_packet[RAW_UDP_HEADER_SIZE + PROTOCOL_PACKET_MAX_SIZE];
    int result = 0;

    while (!signal_handler_stop_requested()) {
        const ssize_t received_size = recvfrom(descriptor, received_packet, sizeof(received_packet), 0,NULL,NULL);

        if (received_size < 0) {
            if (errno == EINTR) continue;

            result = -1;
            break;
        }

        raw_udp_datagram_t datagram;
        if (!raw_udp_parse(received_packet,(size_t)received_size,&datagram)) continue;

        if (datagram.destination_port != server_port) continue;

        protocol_message_t request;
        if (!protocol_decode(datagram.payload,datagram.payload_size,&request)) continue;

        protocol_message_t response;
        bool should_reply = false;
        if (!server_handle_message(&state, datagram.source_ip, datagram.source_port,
            &request, &response, &should_reply))
            continue;

        if (!should_reply) continue;

        size_t encoded_size = 0;
        if (!protocol_encode(&response, encoded_response, sizeof(encoded_response), &encoded_size)) {
            errno = EPROTO;
            result = -1;
            break;
        }

        size_t outgoing_size = 0;
        if (!raw_udp_build(
            outgoing_packet,
            sizeof(outgoing_packet),
            &outgoing_size,
            datagram.destination_ip,
            datagram.source_ip,
            server_port,
            datagram.source_port,
            encoded_response,
            encoded_size
        )) {
            errno = EPROTO;
            result = -1;
            break;
        }

        const ssize_t sent_size = raw_udp_send(descriptor, outgoing_packet, outgoing_size, datagram.source_ip);

        if (sent_size < 0) {
            result = -1;
            break;
        }
        if ((size_t)sent_size != outgoing_size) {
            errno = EIO;
            result = -1;
            break;
        }
    }

    const int saved_errno = errno;
    server_state_destroy(&state);

    if (close(descriptor) < 0 && result == 0) return -1;

    if (result < 0) errno = saved_errno;
    return result;
}
