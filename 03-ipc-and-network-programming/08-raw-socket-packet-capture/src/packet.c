#include "packet.h"

#include <string.h>

#define ETHERNET_HEADER_SIZE 14U
#define ETHERNET_TYPE_IPV4 UINT16_C(0x0800)
#define IPV4_MIN_HEADER_SIZE 20U
#define IPV4_VERSION 4U
#define IPV4_PROTOCOL_UDP 17U
#define IPV4_MORE_FRAGMENTS UINT16_C(0x2000)
#define IPV4_FRAGMENT_OFFSET UINT16_C(0x1FFF)
#define UDP_HEADER_SIZE 8U

static uint16_t read_u16_be(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] << 8U) | data[1];
}

packet_parse_result_t packet_parse_udp(const uint8_t *frame, size_t frame_size, udp_packet_t *packet) {
    if (frame == NULL || packet == NULL) return PACKET_PARSE_MALFORMED;
    if (frame_size < ETHERNET_HEADER_SIZE) return PACKET_PARSE_MALFORMED;

    const uint16_t ethernet_type = read_u16_be(frame + 12U);
    if (ethernet_type != ETHERNET_TYPE_IPV4) return PACKET_PARSE_UNSUPPORTED;

    const uint8_t *ipv4 = frame + ETHERNET_HEADER_SIZE;
    const size_t captured_ipv4_size = frame_size - ETHERNET_HEADER_SIZE;
    if (captured_ipv4_size < IPV4_MIN_HEADER_SIZE) return PACKET_PARSE_MALFORMED;

    const uint8_t version = ipv4[0] >> 4U;
    const size_t ipv4_header_size = (size_t)(ipv4[0] & UINT8_C(0x0F)) * 4U;
    if (version != IPV4_VERSION || ipv4_header_size < IPV4_MIN_HEADER_SIZE) return PACKET_PARSE_MALFORMED;
    if (ipv4_header_size > captured_ipv4_size) return PACKET_PARSE_MALFORMED;

    const size_t ipv4_total_size = read_u16_be(ipv4 + 2U);
    if (ipv4_total_size < ipv4_header_size || ipv4_total_size > captured_ipv4_size) return PACKET_PARSE_MALFORMED;

    const uint16_t fragment_data = read_u16_be(ipv4 + 6U);
    if ((fragment_data & (IPV4_MORE_FRAGMENTS | IPV4_FRAGMENT_OFFSET)) != 0) return PACKET_PARSE_FRAGMENTED;
    if (ipv4[9] != IPV4_PROTOCOL_UDP) return PACKET_PARSE_UNSUPPORTED;

    const size_t ipv4_payload_size = ipv4_total_size - ipv4_header_size;
    if (ipv4_payload_size < UDP_HEADER_SIZE) return PACKET_PARSE_MALFORMED;

    const uint8_t *udp = ipv4 + ipv4_header_size;
    const size_t udp_size = read_u16_be(udp + 4U);
    if (udp_size < UDP_HEADER_SIZE || udp_size > ipv4_payload_size) return PACKET_PARSE_MALFORMED;

    udp_packet_t parsed = {
        .source_port = read_u16_be(udp),
        .destination_port = read_u16_be(udp + 2U),
        .payload = udp + UDP_HEADER_SIZE,
        .payload_size = udp_size - UDP_HEADER_SIZE
    };
    memcpy(parsed.destination_mac, frame, ETHERNET_ADDRESS_SIZE);
    memcpy(parsed.source_mac, frame + ETHERNET_ADDRESS_SIZE, ETHERNET_ADDRESS_SIZE);
    memcpy(&parsed.source_ip.s_addr, ipv4 + 12U, sizeof(parsed.source_ip.s_addr));
    memcpy(&parsed.destination_ip.s_addr, ipv4 + 16U, sizeof(parsed.destination_ip.s_addr));

    *packet = parsed;
    return PACKET_PARSE_OK;
}
