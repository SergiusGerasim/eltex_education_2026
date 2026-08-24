#include "raw_udp.h"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <string.h>
#include <sys/socket.h>

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
    
}

int raw_udp_open(void) {
    return socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
}
