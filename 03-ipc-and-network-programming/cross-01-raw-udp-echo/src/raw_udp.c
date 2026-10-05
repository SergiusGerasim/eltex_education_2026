#include "raw_udp.h"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

_Static_assert(
    sizeof(struct udphdr) == RAW_UDP_HEADER_SIZE,
    "Unexpected UDP header size"
);

static uint32_t checksum_add(uint32_t sum, const uint8_t *data, size_t size) {
    while (size >= 2U) {
        const uint32_t word = ((uint32_t)data[0] << 8U) | (uint32_t)data[1];
        sum += word;
        data += 2U;
        size -= 2U;
    }

    if (size == 1U) sum += (uint32_t)data[0] << 8U;

    return sum;
}

static uint16_t checksum_finish(uint32_t sum) {
    while((sum >> 16U) != 0U){
        sum = (sum & UINT32_C(0xFFFF)) + (sum >> 16U);
    }
    return (uint16_t)~sum;
}

static uint16_t udp_checksum(struct in_addr source_ip, struct in_addr destination_ip, 
    const uint8_t *udp_packet, size_t udp_size) {
    uint8_t pseudo_header[12];
    const uint16_t udp_size_network = htons((uint16_t)udp_size);
    uint32_t sum = 0;

    memcpy(pseudo_header, &source_ip.s_addr, sizeof(source_ip.s_addr));
    memcpy(pseudo_header + 4U, &destination_ip.s_addr, sizeof(destination_ip.s_addr));

    pseudo_header[8] = 0;
    pseudo_header[9] = IPPROTO_UDP;

    memcpy(pseudo_header + 10U, &udp_size_network, sizeof(udp_size_network));
    
    sum = checksum_add(sum, pseudo_header, sizeof(pseudo_header));
    sum = checksum_add(sum, udp_packet, udp_size);
    
    return checksum_finish(sum);
}

int raw_udp_open(void) {
    return socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
}

bool raw_udp_build(uint8_t *packet,  size_t capacity, size_t *packet_size, 
    struct in_addr source_ip, struct in_addr destination_ip, 
    uint16_t source_port, uint16_t destination_port, const uint8_t *payload, size_t payload_size)
{
    if (packet_size == NULL) return false;
    *packet_size = 0;

    if (packet == NULL) return false;
    if (payload_size > 0U && payload == NULL) return false;
    if (payload_size > UINT16_MAX - RAW_UDP_HEADER_SIZE) return false;

    const size_t udp_size = RAW_UDP_HEADER_SIZE + payload_size;
    if (capacity < udp_size) return false;

    struct udphdr header = {
        .source = htons(source_port),
        .dest = htons(destination_port),
        .len = htons((uint16_t)udp_size),
        .check = 0
    };
    memcpy(packet, &header, sizeof(header));

    if (payload_size > 0U){
        memcpy(packet + RAW_UDP_HEADER_SIZE, payload, payload_size);
    }
    uint16_t checksum = udp_checksum(source_ip, destination_ip, packet, udp_size);

    if (checksum == 0U) checksum = UINT16_MAX;

    header.check = htons(checksum);
    memcpy(packet, &header, sizeof(header));
    *packet_size = udp_size;
    return true;
}

bool raw_udp_parse(const uint8_t *packet, size_t packet_size, raw_udp_datagram_t *datagram) 
{
    if (datagram == NULL) return false;
    memset(datagram, 0, sizeof(*datagram));

    if (packet == NULL) return false;
    if (packet_size < sizeof(struct iphdr)) return false;

    struct iphdr ip_header;
    memcpy(&ip_header, packet, sizeof(ip_header));

    if (ip_header.version != 4U) return false;
    if (ip_header.ihl < 5U) return false;
    if (ip_header.protocol != IPPROTO_UDP) return false;

    const size_t ip_header_size = (size_t)ip_header.ihl * 4U;

    if (ip_header_size > packet_size) return false;

    const size_t ip_packet_size = ntohs(ip_header.tot_len);

    if (ip_packet_size > packet_size) return false;
    if (ip_packet_size < ip_header_size + RAW_UDP_HEADER_SIZE) {
        return false;
    }

    struct udphdr udp_header;
    memcpy(
        &udp_header,
        packet + ip_header_size,
        sizeof(udp_header)
    );

    const size_t udp_size = ntohs(udp_header.len);

    if (udp_size < RAW_UDP_HEADER_SIZE) return false;
    if (udp_size != ip_packet_size - ip_header_size) return false;

    struct in_addr source_ip = {
        .s_addr = ip_header.saddr
    };

    struct in_addr destination_ip = {
        .s_addr = ip_header.daddr
    };

    const uint8_t *udp_packet = packet + ip_header_size;

    if (
        udp_header.check != 0U &&
        udp_checksum(
            source_ip,
            destination_ip,
            udp_packet,
            udp_size
        ) != 0U
    ) {
        return false;
    }

    datagram->source_ip = source_ip;
    datagram->destination_ip = destination_ip;
    datagram->source_port = ntohs(udp_header.source);
    datagram->destination_port = ntohs(udp_header.dest);
    datagram->payload = udp_packet + RAW_UDP_HEADER_SIZE;
    datagram->payload_size = udp_size - RAW_UDP_HEADER_SIZE;

    return true;
}

ssize_t raw_udp_send(int descriptor, const uint8_t *packet, size_t packet_size, struct in_addr destination_ip)
{
    if (descriptor < 0 || packet == NULL || packet_size < RAW_UDP_HEADER_SIZE || packet_size > UINT16_MAX){
        errno = EINVAL;
        return -1;
    }
    struct sockaddr_in destination = {
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr = destination_ip
    };

    return sendto(descriptor, packet, packet_size, 0, (const struct sockaddr *)&destination, (socklen_t)sizeof(destination));
}