#pragma once

typedef struct publisher_node {
    long pid;
    struct publisher_node *next;
} publisher_node_t;

int run_broker(void);
