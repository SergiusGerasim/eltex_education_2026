#pragma once
#define MESSAGE_SIZE 100
#define PRIORITY_LEVELS 256

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct Node{
    char message[MESSAGE_SIZE];
    struct Node *next;
} Node;

typedef struct{
    Node *head;
    Node *tail;    
} Queue;

typedef struct{
    Queue queue[PRIORITY_LEVELS];
} PriorityQueue; 


bool priority_queue_init(PriorityQueue *pr_queue);

void priority_queue_free(PriorityQueue *pr_queue);

bool push(PriorityQueue *pr_queue, size_t priority, const char *message);

/* res_message must point to a buffer of at least MESSAGE_SIZE bytes. */
bool peek(const PriorityQueue *pr_queue, char *res_message);

bool pop_first(PriorityQueue *pr_queue, char *res_message);

bool pop_by_priority(PriorityQueue *pr_queue, size_t priority, char *res_message);

bool pop_by_priority_or_upper(PriorityQueue *pr_queue, size_t priority, char *res_message);
