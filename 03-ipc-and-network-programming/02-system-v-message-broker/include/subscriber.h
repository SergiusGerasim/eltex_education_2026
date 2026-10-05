#pragma once

typedef struct subscription_node {
    long subscriber_pid;
    char *topic;
    struct subscription_node *next;
} subscription_node_t;

int run_subscriber(int topic_count, char *const topics[]);
