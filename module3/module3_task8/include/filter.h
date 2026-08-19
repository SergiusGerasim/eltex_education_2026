#pragma once

#include "config.h"
#include "packet.h"

int filter_matches(capture_filter_t filter, const udp_packet_t *packet);
int filter_chat_matches(const udp_packet_t *packet);
int filter_dns_matches(const udp_packet_t *packet);
