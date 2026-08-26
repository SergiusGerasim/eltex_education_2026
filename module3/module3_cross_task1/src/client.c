#include "client.h"
#include "raw_udp.h"
#include "signal_handler.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CLIENT_POLL_TIMEOUT_MS 250

static bool client_is_open(const client_t *client) {
    return client != NULL && client->descriptor >= 0;
}

static bool client_send_message(client_t *client,const protocol_message_t *message) {
    if (!client_is_open(client) || message == NULL) {
        errno = EINVAL;
        return false;
    }

    uint8_t encoded_message[PROTOCOL_PACKET_MAX_SIZE];
    uint8_t udp_packet[RAW_UDP_HEADER_SIZE + PROTOCOL_PACKET_MAX_SIZE];
    size_t encoded_size = 0;
    size_t packet_size = 0;

    if (!protocol_encode(message,encoded_message,sizeof(encoded_message),&encoded_size)) {
        errno = EPROTO;
        return false;
    }
    if (!raw_udp_build(udp_packet,sizeof(udp_packet),&packet_size,client->local_ip,client->server_ip,client->local_port,
        client->server_port,encoded_message,encoded_size)) {
        errno = EPROTO;
        return false;
    }

    const ssize_t sent_size = raw_udp_send(client->descriptor,udp_packet,packet_size,client->server_ip);
    if (sent_size < 0) return false;
    if ((size_t)sent_size == packet_size) return true;

    errno = EIO;
    return false;
}

static bool response_matches(const client_t *client,const raw_udp_datagram_t *datagram,const protocol_message_t *response,
    const uint8_t *payload,size_t payload_size) {
    if (datagram->source_ip.s_addr != client->server_ip.s_addr) return false;
    if (datagram->destination_ip.s_addr != client->local_ip.s_addr) return false;
    if (datagram->source_port != client->server_port || datagram->destination_port != client->local_port) return false;
    if (response->type != ECHO_RESPONSE || response->payload_size != payload_size) return false;
    return payload_size == 0U || memcmp(response->payload,payload,payload_size) == 0;
}

static bool client_receive_response(client_t *client,const uint8_t *payload,size_t payload_size,protocol_message_t *response) {
    uint8_t packet[UINT16_MAX];
    struct pollfd event = {.fd = client->descriptor,.events = POLLIN};

    while (!signal_handler_stop_requested()) {
        event.revents = 0;
        const int poll_result = poll(&event,1,CLIENT_POLL_TIMEOUT_MS);
        if (poll_result < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (poll_result == 0) continue;
        if ((event.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            errno = EIO;
            return false;
        }
        if ((event.revents & POLLIN) == 0) continue;

        const ssize_t received_size = recvfrom(client->descriptor,packet,sizeof(packet),0,NULL,NULL);
        if (received_size < 0) {
            if (errno == EINTR) continue;
            return false;
        }

        raw_udp_datagram_t datagram;
        protocol_message_t message;
        if (!raw_udp_parse(packet,(size_t)received_size,&datagram)) continue;
        if (!protocol_decode(datagram.payload,datagram.payload_size,&message)) continue;
        if (!response_matches(client,&datagram,&message,payload,payload_size)) continue;

        *response = message;
        return true;
    }

    errno = EINTR;
    return false;
}

void client_init(client_t *client) {
    if (client == NULL) return;
    memset(client,0,sizeof(*client));
    client->descriptor = -1;
}

bool client_open(client_t *client,struct in_addr local_ip,uint16_t local_port,struct in_addr server_ip,uint16_t server_port) {
    if (client == NULL || client->descriptor >= 0 || local_port == 0U || server_port == 0U) {
        errno = EINVAL;
        return false;
    }

    const int descriptor = raw_udp_open();
    if (descriptor < 0) return false;

    const struct sockaddr_in local_address = {.sin_family = AF_INET,.sin_port = 0,.sin_addr = local_ip};
    if (bind(descriptor,(const struct sockaddr *)&local_address,(socklen_t)sizeof(local_address)) == -1) {
        const int saved_errno = errno;
        close(descriptor);
        errno = saved_errno;
        return false;
    }

    client->descriptor = descriptor;
    client->local_ip = local_ip;
    client->server_ip = server_ip;
    client->local_port = local_port;
    client->server_port = server_port;
    return true;
}

bool client_send_request(client_t *client,const uint8_t *payload,size_t payload_size,protocol_message_t *response) {
    if (!client_is_open(client) || response == NULL || payload_size > PROTOCOL_PAYLOAD_MAX || (payload_size > 0U && payload == NULL)) {
        errno = EINVAL;
        return false;
    }
    if (signal_handler_stop_requested()) {
        errno = EINTR;
        return false;
    }

    protocol_message_t request = {.type = ECHO_REQUEST,.counter = 0U,.payload_size = payload_size};
    if (payload_size > 0U) memcpy(request.payload,payload,payload_size);
    memset(response,0,sizeof(*response));

    if (!client_send_message(client,&request)) return false;
    return client_receive_response(client,payload,payload_size,response);
}

bool client_send_close(client_t *client) {
    const protocol_message_t message = {.type = ECHO_CLOSE,.counter = 0U,.payload_size = 0U};
    return client_send_message(client,&message);
}

void client_close(client_t *client) {
    if (!client_is_open(client)) return;
    close(client->descriptor);
    client->descriptor = -1;
}
