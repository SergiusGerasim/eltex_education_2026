#include "server.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int parse_address(const char *text, struct in_addr *address) {
    return inet_pton(AF_INET, text, address) == 1;
}

static int next_counter_is(
    server_state_t *state,
    struct in_addr ip,
    uint16_t port,
    uint32_t expected
) {
    uint32_t actual = 0;

    return server_state_next_counter(state, ip, port, &actual) &&
           actual == expected;
}

static int test_independent_counters_and_close(void) {
    struct in_addr first_ip;
    struct in_addr second_ip;
    server_state_t state;
    int passed = 0;

    if (!parse_address("192.0.2.1", &first_ip)) return 0;
    if (!parse_address("198.51.100.2", &second_ip)) return 0;

    server_state_init(&state);

    if (!next_counter_is(&state, first_ip, UINT16_C(40000), 1U)) goto cleanup;
    if (!next_counter_is(&state, first_ip, UINT16_C(40000), 2U)) goto cleanup;

    /* The same IP with another port is a different client. */
    if (!next_counter_is(&state, first_ip, UINT16_C(40001), 1U)) goto cleanup;

    /* The same port with another IP is also a different client. */
    if (!next_counter_is(&state, second_ip, UINT16_C(40000), 1U)) goto cleanup;

    if (!server_state_remove_client(&state, first_ip, UINT16_C(40000))) {
        goto cleanup;
    }
    if (server_state_remove_client(&state, first_ip, UINT16_C(40000))) {
        goto cleanup;
    }

    /* ECHO_CLOSE must reset this client's counter. */
    if (!next_counter_is(&state, first_ip, UINT16_C(40000), 1U)) goto cleanup;

    passed = 1;

cleanup:
    server_state_destroy(&state);
    if (state.clients != NULL) return 0;
    return passed;
}

static int test_invalid_arguments(void) {
    struct in_addr ip = {.s_addr = htonl(INADDR_LOOPBACK)};
    server_state_t state;
    uint32_t counter = 99U;

    server_state_init(&state);

    if (server_state_next_counter(NULL, ip, UINT16_C(40000), &counter)) {
        return 0;
    }
    if (server_state_next_counter(&state, ip, UINT16_C(40000), NULL)) {
        return 0;
    }
    if (server_state_remove_client(NULL, ip, UINT16_C(40000))) return 0;
    if (server_state_remove_client(&state, ip, UINT16_C(40000))) return 0;

    server_state_destroy(&state);
    return 1;
}

static int response_is(
    const protocol_message_t *response,
    uint32_t expected_counter,
    const uint8_t *expected_payload,
    size_t expected_payload_size
) {
    return response->type == ECHO_RESPONSE &&
           response->counter == expected_counter &&
           response->payload_size == expected_payload_size &&
           memcmp(
               response->payload,
               expected_payload,
               expected_payload_size
           ) == 0;
}

static int test_handle_message(void) {
    static const uint8_t payload[] = {'p', 'i', 'n', 'g'};
    struct in_addr client_ip;
    server_state_t state;
    protocol_message_t request = {
        .type = ECHO_REQUEST,
        .counter = 0U,
        .payload_size = sizeof(payload)
    };
    protocol_message_t response;
    bool should_reply = false;
    int passed = 0;

    if (!parse_address("192.0.2.1", &client_ip)) return 0;
    memcpy(request.payload, payload, sizeof(payload));
    server_state_init(&state);

    if (!server_handle_message(
        &state,
        client_ip,
        UINT16_C(40000),
        &request,
        &response,
        &should_reply
    )) {
        goto cleanup;
    }
    if (!should_reply || !response_is(&response, 1U, payload, sizeof(payload))) {
        goto cleanup;
    }

    if (!server_handle_message(
        &state,
        client_ip,
        UINT16_C(40000),
        &request,
        &response,
        &should_reply
    )) {
        goto cleanup;
    }
    if (!should_reply || !response_is(&response, 2U, payload, sizeof(payload))) {
        goto cleanup;
    }

    const protocol_message_t close_message = {
        .type = ECHO_CLOSE,
        .counter = 0U,
        .payload_size = 0U
    };

    if (!server_handle_message(
        &state,
        client_ip,
        UINT16_C(40000),
        &close_message,
        &response,
        &should_reply
    )) {
        goto cleanup;
    }
    if (should_reply) goto cleanup;

    if (!server_handle_message(
        &state,
        client_ip,
        UINT16_C(40000),
        &request,
        &response,
        &should_reply
    )) {
        goto cleanup;
    }
    if (!should_reply || !response_is(&response, 1U, payload, sizeof(payload))) {
        goto cleanup;
    }

    passed = 1;

cleanup:
    server_state_destroy(&state);
    return passed;
}

int main(void) {
    if (!test_independent_counters_and_close()) {
        fprintf(stderr, "Server client counters test failed\n");
        return 1;
    }
    if (!test_invalid_arguments()) {
        fprintf(stderr, "Server state invalid arguments test failed\n");
        return 1;
    }
    if (!test_handle_message()) {
        fprintf(stderr, "Server message handling test failed\n");
        return 1;
    }

    puts("Server state tests passed");
    return 0;
}
