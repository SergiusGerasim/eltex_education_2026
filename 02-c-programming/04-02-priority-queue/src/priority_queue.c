#include "priority_queue.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool priority_queue_init(PriorityQueue *pr_queue){

    if (pr_queue == NULL) return false;

    for (size_t i = 0; i < PRIORITY_LEVELS; i++){
        pr_queue->queue[i].head = NULL;
        pr_queue->queue[i].tail = NULL;
    }

    return true;
}

void priority_queue_free(PriorityQueue *pr_queue){
    if (pr_queue == NULL) return;

    for (size_t i = 0; i < PRIORITY_LEVELS; i++){
        while (pr_queue->queue[i].head != NULL){
            Node *node = pr_queue->queue[i].head;
            pr_queue->queue[i].head = node->next;
            free(node);
        }
        pr_queue->queue[i].tail = NULL;
    }
}

static Node *create_node(const char *message){
    Node *node = malloc(sizeof(*node));
    if (node == NULL) return NULL;
    
    int written = snprintf(node->message, sizeof(node->message), "%s", message);

    if (written < 0 || (size_t)written >= sizeof(node->message)) {
        free(node);
        return NULL;
    }

    node->next = NULL;
    return node;
}

static bool copy_message(char *destination, const char *source){
    int written = snprintf(destination, MESSAGE_SIZE, "%s", source);
    return written >= 0 && (size_t)written < MESSAGE_SIZE;
}

static bool pop_from_queue(Queue *queue, char *res_message){
    if (queue == NULL || res_message == NULL || queue->head == NULL) return false;
    if (!copy_message(res_message, queue->head->message)) return false;

    Node *node = queue->head;
    queue->head = node->next;
    if (queue->head == NULL) queue->tail = NULL;
    free(node);
    return true;
}

bool push(PriorityQueue *pr_queue, size_t priority, const char *message){
    
    if (pr_queue == NULL || message == NULL || priority >= PRIORITY_LEVELS) return false;

    Queue *temp_queue = &pr_queue->queue[priority];
    Node *new_node = create_node(message);
    if (new_node == NULL) return false;

    if (temp_queue->head == NULL){
        temp_queue->head = new_node;
        temp_queue->tail = new_node;
    }
    else{
        temp_queue->tail->next = new_node;
        temp_queue->tail = new_node;
    }

    return true;
}

bool peek(const PriorityQueue *pr_queue, char *res_message){
    if (pr_queue == NULL || res_message == NULL) return false;

    for (size_t priority = PRIORITY_LEVELS; priority-- > 0;){
        if (pr_queue->queue[priority].head != NULL){
            return copy_message(res_message, pr_queue->queue[priority].head->message);
        }
    }
    return false;
}

bool pop_first(PriorityQueue *pr_queue, char *res_message){
    if (pr_queue == NULL || res_message == NULL) return false;

    for (size_t priority = PRIORITY_LEVELS; priority-- > 0;){
        if (pr_queue->queue[priority].head != NULL){
            return pop_from_queue(&pr_queue->queue[priority], res_message);
        }
    }
    return false;
}

bool pop_by_priority(PriorityQueue *pr_queue, size_t priority, char *res_message){
    if (pr_queue == NULL || res_message == NULL || priority >= PRIORITY_LEVELS) return false;
    return pop_from_queue(&pr_queue->queue[priority], res_message);
}

bool pop_by_priority_or_upper(PriorityQueue *pr_queue, size_t priority, char *res_message){
    if (pr_queue == NULL || res_message == NULL || priority >= PRIORITY_LEVELS) return false;

    for (size_t current = PRIORITY_LEVELS; current-- > priority;){
        if (pr_queue->queue[current].head != NULL){
            return pop_from_queue(&pr_queue->queue[current], res_message);
        }
    }
    return false;
}
