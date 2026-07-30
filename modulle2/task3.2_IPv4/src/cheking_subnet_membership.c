#include "cheking_subnet_membership.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

execution_status_t ipv4_parse(const char *text, ipv4_address_t *res)
{
    if (text == NULL || res == NULL) return EXECUTION_INVALID_INPUT;

    ipv4_address_t parsed_address = {0};
    const char *cursor = text;

    for (size_t i = 0; i < 4; ++i) {
        if (!isdigit((unsigned char)*cursor)) return EXECUTION_INVALID_INPUT;

        unsigned int value = 0;
        size_t digit_count = 0;

        while (isdigit((unsigned char)*cursor)) {
            value = value * 10U + (unsigned int)(*cursor - '0');
            ++digit_count;

            if (digit_count > 3 || value > UINT8_MAX) return EXECUTION_INVALID_INPUT;

            ++cursor;
        }

        parsed_address.octets[i] = (uint8_t)value;

        if (i < 3) {
            if (*cursor != '.') return EXECUTION_INVALID_INPUT;
            ++cursor;
        }
    }

    if (*cursor != '\0') return EXECUTION_INVALID_INPUT;

    *res = parsed_address;
    return EXECUTION_OK;
}

uint32_t ipv4_to_uint32(const ipv4_address_t *address){
    return ((uint32_t)address->octets[0] << 24) | ((uint32_t)address->octets[1] << 16) | 
    ((uint32_t)address->octets[2] << 8) | ((uint32_t)address->octets[3]);
}

bool ipv4_mask_is_valid(const ipv4_address_t *mask)
{
    if (mask == NULL) return false;

    uint32_t inverted_mask = ~ipv4_to_uint32(mask);
    return (inverted_mask & (inverted_mask + UINT32_C(1))) == 0;
}

bool subnet_membership(const ipv4_address_t *address, const ipv4_address_t *subnet_address, const ipv4_address_t *subnet_mask){
    
    if (address == NULL || subnet_address == NULL || subnet_mask == NULL) return false;
    
    uint32_t ip_src = ipv4_to_uint32(subnet_address); 
    uint32_t ip_dst = ipv4_to_uint32(address);
    uint32_t mask = ipv4_to_uint32(subnet_mask);

    uint32_t net_src = ip_src & mask;
    uint32_t net_dst = ip_dst & mask;

    uint32_t result = net_src ^ net_dst;
    
    if (result == 0) return true;
    return false;
}

execution_status_t process_packets(
    const ipv4_address_t *packets,
    size_t count,
    const ipv4_address_t *subnet_address,
    const ipv4_address_t *subnet_mask,
    packet_statistics_t *statistics)
{
    if (packets == NULL || count == 0 || subnet_address == NULL ||
        subnet_mask == NULL || statistics == NULL) {
        return EXECUTION_INVALID_INPUT;
    }

    packet_statistics_t result = {
        .membership_count = 0,
        .all_count = count
    };

    for (size_t i = 0; i < count; ++i) {
        if (subnet_membership(&packets[i], subnet_address, subnet_mask)) {
            ++result.membership_count;
        }
    }

    *statistics = result;
    return EXECUTION_OK;
}

execution_status_t generate_packets(size_t count, ipv4_address_t **packets){
    if (packets == NULL || count == 0) return EXECUTION_INVALID_INPUT;

    if (count > SIZE_MAX / sizeof(**packets)) return EXECUTION_ALLOCATION_ERROR;
    
    ipv4_address_t *generated = malloc(sizeof(**packets) * count);
    if (generated == NULL) return EXECUTION_ALLOCATION_ERROR;

    for (size_t i = 0; i < count; i++){
        for (size_t octet = 0; octet < 4; octet++){
            generated[i].octets[octet] = (uint8_t)(rand()%256);
        }
    }
    *packets = generated;
    return EXECUTION_OK;
}

execution_status_t ipv4_to_string(const ipv4_address_t *address, char *buffer, size_t buffer_size){
    if (address == NULL || buffer == NULL || buffer_size == 0) return EXECUTION_INVALID_INPUT;
    
    int written = snprintf(buffer, buffer_size, "%u.%u.%u.%u", 
        (unsigned int)address->octets[0], (unsigned int)address->octets[1], (unsigned int)address->octets[2], (unsigned int)address->octets[3]);
    
    if (written < 0 || (size_t)written >= buffer_size) return EXECUTION_FAIL;
    return EXECUTION_OK; 
}
