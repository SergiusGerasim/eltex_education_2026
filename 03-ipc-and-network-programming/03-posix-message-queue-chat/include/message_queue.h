#pragma once

#include <mqueue.h>
#include <stdbool.h>
#include <sys/types.h>

#define CHAT_MESSAGE_SIZE 1024
#define CHAT_MAX_MESSAGES 10
#define CHAT_MESSAGE_PRIORITY 1U
#define CHAT_EXIT_PRIORITY 10U
#define QUEUE_NAME_SIZE 256

typedef struct {
    mqd_t receive_queue;
    mqd_t send_queue;
    char first_name[QUEUE_NAME_SIZE];
    char second_name[QUEUE_NAME_SIZE];
    bool is_owner;
} message_queue_pair_t;

int message_queue_pair_set_names(message_queue_pair_t *pair, const char *base_name);
int message_queue_pair_open(message_queue_pair_t *pair, const char *base_name);
void message_queue_pair_close(message_queue_pair_t *pair);
int message_queue_send(mqd_t queue, const char *message, unsigned int priority);
ssize_t message_queue_receive(mqd_t queue, char *buffer, size_t buffer_size, unsigned int *priority);
ssize_t message_queue_receive_timed(mqd_t queue, char *buffer, size_t buffer_size, unsigned int *priority, long timeout_ms);
