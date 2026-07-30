#pragma once 

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define IPV4_STRING_SIZE 16

typedef enum {
    EXECUTION_OK = 0,
    EXECUTION_INVALID_INPUT,
    EXECUTION_ALLOCATION_ERROR,
    EXECUTION_FAIL
} execution_status_t;

typedef struct {
    uint8_t octets[4];
} ipv4_address_t;

typedef struct{
    size_t membership_count;
    size_t all_count; 
} packet_statistics_t;

execution_status_t ipv4_parse(const char *text, ipv4_address_t *res);

bool subnet_membership(const ipv4_address_t *address, const ipv4_address_t *subnet_address, const ipv4_address_t *subnet_mask);

bool ipv4_mask_is_valid(const ipv4_address_t *mask);

execution_status_t process_packets(
    const ipv4_address_t *packets,
    size_t count,
    const ipv4_address_t *subnet_address,
    const ipv4_address_t *subnet_mask,
    packet_statistics_t *statistics
);

execution_status_t generate_packets(size_t count, ipv4_address_t **packets);

execution_status_t ipv4_to_string(const ipv4_address_t *address, char *buffer, size_t buffer_size);

uint32_t ipv4_to_uint32(const ipv4_address_t *address);
