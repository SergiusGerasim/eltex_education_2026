#pragma once
#define MESSAGE_TEXT_SIZE 512
#define MESSAGE_QUEUE_KEY 1234
#define BROKER_MESSAGE_TYPE 1L

typedef struct {
    long priority; // must be greater than 0
    char text[MESSAGE_TEXT_SIZE];
} message_t;

typedef struct {
    char *command;
    long sender_pid;
    char *topic;
    char *payload;
} parsed_message_t;