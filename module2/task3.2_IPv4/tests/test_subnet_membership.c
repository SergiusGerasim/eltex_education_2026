#include "unity.h"
#include "cheking_subnet_membership.h"

#include <stdlib.h>
#include <stdint.h>

static packet_statistics_t statistic;
static ipv4_address_t *address_array;


void setUp(void){
    address_array = NULL;
    statistic.all_count = 0;
    statistic.membership_count = 0;
}

void tearDown(void){
    free(address_array);
}

void test_ipv4_parse_should_parse_valid_address(void){
    const char text[] = "225.0.14.29";
    ipv4_address_t parse_result = {0};
    execution_status_t status = ipv4_parse(text, &parse_result);

    TEST_ASSERT_EQUAL_INT(EXECUTION_OK, status);

    TEST_ASSERT_EQUAL_UINT8(225, parse_result.octets[0]);
    TEST_ASSERT_EQUAL_UINT8(0, parse_result.octets[1]);
    TEST_ASSERT_EQUAL_UINT8(14, parse_result.octets[2]);
    TEST_ASSERT_EQUAL_UINT8(29, parse_result.octets[3]);
}

void test_ipv4_parse_should_parse_boundary_addresses(void){
    ipv4_address_t result = {0};

    TEST_ASSERT_EQUAL_INT(EXECUTION_OK, ipv4_parse("0.0.0.0", &result));
    TEST_ASSERT_EACH_EQUAL_UINT8(0, result.octets, 4);

    TEST_ASSERT_EQUAL_INT(EXECUTION_OK, ipv4_parse("255.255.255.255", &result));
    TEST_ASSERT_EACH_EQUAL_UINT8(255, result.octets, 4);
}

void test_ipv4_parse_should_reject_invalid_input(void){
    static const char *invalid_addresses[] = {
        "",
        "192.168.1",
        "192.168.1.1.2",
        "192..1.1",
        ".192.168.1",
        "192.168.1.",
        "256.168.1.1",
        "192.168.0001.1",
        "192.168.-1.1",
        "192.168.1x.1",
        " 192.168.1.1",
        "192.168.1.1 "
    };
    ipv4_address_t result = {
        .octets = {1, 2, 3, 4}
    };

    for (size_t i = 0; i < sizeof(invalid_addresses) / sizeof(invalid_addresses[0]); ++i) {
        TEST_ASSERT_EQUAL_INT(
            EXECUTION_INVALID_INPUT,
            ipv4_parse(invalid_addresses[i], &result)
        );
        TEST_ASSERT_EQUAL_UINT8_ARRAY(
            ((const uint8_t[]){1, 2, 3, 4}),
            result.octets,
            4
        );
    }

    TEST_ASSERT_EQUAL_INT(EXECUTION_INVALID_INPUT, ipv4_parse(NULL, &result));
    TEST_ASSERT_EQUAL_INT(EXECUTION_INVALID_INPUT, ipv4_parse("1.2.3.4", NULL));
}

void test_ipv4_to_uint32_should_pack_octets_in_network_order(void){
    const ipv4_address_t address = {
        .octets = {192, 168, 1, 10}
    };

    TEST_ASSERT_EQUAL_HEX32(UINT32_C(0xC0A8010A), ipv4_to_uint32(&address));
}

void test_subnet_membership_should_identify_local_and_external_addresses(void){
    const ipv4_address_t gateway = {
        .octets = {192, 168, 1, 10}
    };
    const ipv4_address_t mask = {
        .octets = {255, 255, 255, 0}
    };
    const ipv4_address_t local = {
        .octets = {192, 168, 1, 200}
    };
    const ipv4_address_t external = {
        .octets = {192, 168, 2, 1}
    };

    TEST_ASSERT_TRUE(subnet_membership(&local, &gateway, &mask));
    TEST_ASSERT_FALSE(subnet_membership(&external, &gateway, &mask));
}

void test_subnet_membership_should_support_host_and_default_masks(void){
    const ipv4_address_t gateway = {
        .octets = {10, 20, 30, 40}
    };
    const ipv4_address_t same_address = {
        .octets = {10, 20, 30, 40}
    };
    const ipv4_address_t other_address = {
        .octets = {1, 2, 3, 4}
    };
    const ipv4_address_t host_mask = {
        .octets = {255, 255, 255, 255}
    };
    const ipv4_address_t default_mask = {
        .octets = {0, 0, 0, 0}
    };

    TEST_ASSERT_TRUE(subnet_membership(&same_address, &gateway, &host_mask));
    TEST_ASSERT_FALSE(subnet_membership(&other_address, &gateway, &host_mask));
    TEST_ASSERT_TRUE(subnet_membership(&other_address, &gateway, &default_mask));
}

void test_subnet_membership_should_reject_null_arguments(void){
    const ipv4_address_t address = {
        .octets = {192, 168, 1, 1}
    };
    const ipv4_address_t mask = {
        .octets = {255, 255, 255, 0}
    };

    TEST_ASSERT_FALSE(subnet_membership(NULL, &address, &mask));
    TEST_ASSERT_FALSE(subnet_membership(&address, NULL, &mask));
    TEST_ASSERT_FALSE(subnet_membership(&address, &address, NULL));
}

void test_ipv4_mask_is_valid_should_accept_contiguous_masks(void){
    const ipv4_address_t masks[] = {
        {.octets = {0, 0, 0, 0}},
        {.octets = {128, 0, 0, 0}},
        {.octets = {255, 255, 255, 0}},
        {.octets = {255, 255, 255, 255}}
    };

    for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); ++i) {
        TEST_ASSERT_TRUE(ipv4_mask_is_valid(&masks[i]));
    }
}

void test_ipv4_mask_is_valid_should_reject_noncontiguous_masks(void){
    const ipv4_address_t masks[] = {
        {.octets = {255, 0, 255, 0}},
        {.octets = {255, 127, 0, 0}},
        {.octets = {255, 255, 255, 1}}
    };

    for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); ++i) {
        TEST_ASSERT_FALSE(ipv4_mask_is_valid(&masks[i]));
    }

    TEST_ASSERT_FALSE(ipv4_mask_is_valid(NULL));
}

void test_generate_packets_should_generate_valid_addresses(void){
    const size_t packet_count = 1000;

    execution_status_t status = generate_packets(packet_count, &address_array);

    TEST_ASSERT_EQUAL_INT(EXECUTION_OK, status);
    TEST_ASSERT_NOT_NULL(address_array);

    for (size_t i = 0; i < packet_count; ++i) {
        char text[IPV4_STRING_SIZE];
        ipv4_address_t parsed_address = {0};

        status = ipv4_to_string(&address_array[i], text, sizeof(text));
        TEST_ASSERT_EQUAL_INT(EXECUTION_OK, status);

        status = ipv4_parse(text, &parsed_address);
        TEST_ASSERT_EQUAL_INT(EXECUTION_OK, status);

        TEST_ASSERT_EQUAL_UINT8_ARRAY(
            address_array[i].octets,
            parsed_address.octets,
            4
        );
    }
}

void test_generate_packets_should_reject_invalid_arguments(void){
    ipv4_address_t *packets = NULL;
    const size_t overflowing_count =
        SIZE_MAX / sizeof(*packets) + 1;

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        generate_packets(0, &packets)
    );
    TEST_ASSERT_NULL(packets);

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        generate_packets(1, NULL)
    );

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_ALLOCATION_ERROR,
        generate_packets(overflowing_count, &packets)
    );
    TEST_ASSERT_NULL(packets);
}

void test_ipv4_to_string_should_format_boundary_addresses(void){
    const ipv4_address_t minimum = {
        .octets = {0, 0, 0, 0}
    };
    const ipv4_address_t maximum = {
        .octets = {255, 255, 255, 255}
    };
    char buffer[IPV4_STRING_SIZE];

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_OK,
        ipv4_to_string(&minimum, buffer, sizeof(buffer))
    );
    TEST_ASSERT_EQUAL_STRING("0.0.0.0", buffer);

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_OK,
        ipv4_to_string(&maximum, buffer, sizeof(buffer))
    );
    TEST_ASSERT_EQUAL_STRING("255.255.255.255", buffer);
}

void test_ipv4_to_string_should_reject_invalid_arguments_and_small_buffer(void){
    const ipv4_address_t address = {
        .octets = {192, 168, 1, 10}
    };
    char small_buffer[8];
    char buffer[IPV4_STRING_SIZE];

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        ipv4_to_string(NULL, buffer, sizeof(buffer))
    );
    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        ipv4_to_string(&address, NULL, sizeof(buffer))
    );
    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        ipv4_to_string(&address, buffer, 0)
    );
    TEST_ASSERT_EQUAL_INT(
        EXECUTION_FAIL,
        ipv4_to_string(&address, small_buffer, sizeof(small_buffer))
    );
}

void test_process_packets_should_count_local_and_external_packets(void){
    const ipv4_address_t subnet_address = {
        .octets = {192, 168, 1, 10}
    };
    const ipv4_address_t subnet_mask = {
        .octets = {255, 255, 255, 0}
    };
    const ipv4_address_t packets[] = {
        {.octets = {192, 168, 1, 1}},
        {.octets = {192, 168, 2, 1}},
        {.octets = {192, 168, 1, 200}}
    };

    execution_status_t status = process_packets(
        packets,
        sizeof(packets) / sizeof(packets[0]),
        &subnet_address,
        &subnet_mask,
        &statistic
    );

    TEST_ASSERT_EQUAL_INT(EXECUTION_OK, status);
    TEST_ASSERT_EQUAL_size_t(3, statistic.all_count);
    TEST_ASSERT_EQUAL_size_t(2, statistic.membership_count);
}

void test_process_packets_should_handle_all_local_and_all_external_packets(void){
    const ipv4_address_t gateway = {
        .octets = {10, 0, 0, 1}
    };
    const ipv4_address_t mask = {
        .octets = {255, 0, 0, 0}
    };
    const ipv4_address_t local_packets[] = {
        {.octets = {10, 1, 2, 3}},
        {.octets = {10, 255, 255, 254}}
    };
    const ipv4_address_t external_packets[] = {
        {.octets = {11, 0, 0, 1}},
        {.octets = {192, 168, 1, 1}}
    };

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_OK,
        process_packets(local_packets, 2, &gateway, &mask, &statistic)
    );
    TEST_ASSERT_EQUAL_size_t(2, statistic.all_count);
    TEST_ASSERT_EQUAL_size_t(2, statistic.membership_count);

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_OK,
        process_packets(external_packets, 2, &gateway, &mask, &statistic)
    );
    TEST_ASSERT_EQUAL_size_t(2, statistic.all_count);
    TEST_ASSERT_EQUAL_size_t(0, statistic.membership_count);
}

void test_process_packets_should_reject_invalid_arguments(void){
    const ipv4_address_t address = {
        .octets = {192, 168, 1, 1}
    };
    const ipv4_address_t mask = {
        .octets = {255, 255, 255, 0}
    };

    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        process_packets(NULL, 1, &address, &mask, &statistic)
    );
    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        process_packets(&address, 0, &address, &mask, &statistic)
    );
    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        process_packets(&address, 1, NULL, &mask, &statistic)
    );
    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        process_packets(&address, 1, &address, NULL, &statistic)
    );
    TEST_ASSERT_EQUAL_INT(
        EXECUTION_INVALID_INPUT,
        process_packets(&address, 1, &address, &mask, NULL)
    );
}

int main(void){
    UNITY_BEGIN();

    RUN_TEST(test_ipv4_parse_should_parse_valid_address);
    RUN_TEST(test_ipv4_parse_should_parse_boundary_addresses);
    RUN_TEST(test_ipv4_parse_should_reject_invalid_input);
    RUN_TEST(test_ipv4_to_uint32_should_pack_octets_in_network_order);
    RUN_TEST(test_subnet_membership_should_identify_local_and_external_addresses);
    RUN_TEST(test_subnet_membership_should_support_host_and_default_masks);
    RUN_TEST(test_subnet_membership_should_reject_null_arguments);
    RUN_TEST(test_ipv4_mask_is_valid_should_accept_contiguous_masks);
    RUN_TEST(test_ipv4_mask_is_valid_should_reject_noncontiguous_masks);
    RUN_TEST(test_generate_packets_should_generate_valid_addresses);
    RUN_TEST(test_generate_packets_should_reject_invalid_arguments);
    RUN_TEST(test_ipv4_to_string_should_format_boundary_addresses);
    RUN_TEST(test_ipv4_to_string_should_reject_invalid_arguments_and_small_buffer);
    RUN_TEST(test_process_packets_should_count_local_and_external_packets);
    RUN_TEST(test_process_packets_should_handle_all_local_and_all_external_packets);
    RUN_TEST(test_process_packets_should_reject_invalid_arguments);

    return UNITY_END();
}
