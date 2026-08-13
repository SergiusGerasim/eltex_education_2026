#include "chat.h"
#include "message_queue.h"
#include "signal_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdatomic.h>
#include <signal.h>
#include <errno.h>

#define RECEIVE_TIMEOUT_MS 100L

typedef struct {
    message_queue_pair_t *pair;
    pthread_t main_thread;
    atomic_bool stop_requested;
} chat_context_t;

static void *receive_messages(void *argument) {
    chat_context_t *context = argument;
    message_queue_pair_t *pair = context->pair;
    char buffer[CHAT_MESSAGE_SIZE];
    unsigned int priority;

    while (!atomic_load(&context->stop_requested)) {
        ssize_t received = message_queue_receive_timed(pair->receive_queue, buffer, sizeof(buffer), &priority, RECEIVE_TIMEOUT_MS);

        if (received == -1) {
            if (errno == ETIMEDOUT || errno == EINTR) continue;

            perror("mq_timedreceive");
            atomic_store(&context->stop_requested, true);

            int status = pthread_kill(context->main_thread, SIGINT);
            if (status != 0) fprintf(stderr, "pthread_kill: %s\n", strerror(status));
            return NULL;
        }

        if (priority == CHAT_EXIT_PRIORITY) {
            atomic_store(&context->stop_requested, true);

            printf("Peer disconnected\n");

            int status = pthread_kill(context->main_thread, SIGINT);
            if (status != 0) fprintf(stderr, "pthread_kill: %s\n", strerror(status));

            return NULL;
        }

        printf("Peer: %s\n", buffer);
    }

    return NULL;
}

int chat_run(const char *queue_base_name) {
    message_queue_pair_t pair;

    setvbuf(stdout, NULL, _IOLBF, 0);

    if (signal_handler_install() == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    if (message_queue_pair_open(&pair, queue_base_name) == -1) return EXIT_FAILURE;

    if (pair.is_owner){
        printf("Queues created: %s, %s\n", pair.first_name, pair.second_name);
    } else {
        printf("Connected to queues: %s, %s\n", pair.first_name, pair.second_name);
    }

    pthread_t receiver_thread;

    chat_context_t context = {
        .pair = &pair,
        .main_thread = pthread_self()
    };

    atomic_init(&context.stop_requested, false);

    sigset_t sigint_set;

    sigemptyset(&sigint_set);
    sigaddset(&sigint_set, SIGINT);

    int status = pthread_sigmask(SIG_BLOCK, &sigint_set, NULL);

    if (status != 0) {
        fprintf(stderr, "pthread_sigmask: %s\n", strerror(status));
        atomic_store(&context.stop_requested, true);
        pthread_join(receiver_thread, NULL);
        message_queue_pair_close(&pair);
        return EXIT_FAILURE;
        message_queue_pair_close(&pair);
        return EXIT_FAILURE;
    }

    status = pthread_create(&receiver_thread, NULL, receive_messages, &context);
    if (status != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(status));
        pthread_sigmask(SIG_UNBLOCK, &sigint_set, NULL);
        message_queue_pair_close(&pair);
        return -1;
    }
    status = pthread_sigmask(SIG_UNBLOCK, &sigint_set, NULL);

    if (status != 0) {
        fprintf(stderr, "pthread_sigmask: %s\n", strerror(status));
    }
    char message[CHAT_MESSAGE_SIZE];

    while (!signal_handler_stop_requested() && fgets(message, sizeof(message), stdin) != NULL) {
        message[strcspn(message, "\n")] = '\0';

        if (strcmp(message, "/exit") == 0) {
            if (!atomic_exchange(&context.stop_requested, true)) {
                message_queue_send(pair.send_queue, "", CHAT_EXIT_PRIORITY);
            }

            break;
        }

        if (message_queue_send(pair.send_queue, message, CHAT_MESSAGE_PRIORITY) == -1) {
            atomic_store(&context.stop_requested, true);
            break;
        }
    }

    if (!atomic_exchange(&context.stop_requested, true)) {
        message_queue_send(pair.send_queue, "", CHAT_EXIT_PRIORITY);
    }

    status = pthread_join(receiver_thread, NULL);

    if (status != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(status));
        message_queue_pair_close(&pair);
        return EXIT_FAILURE;
    }

    message_queue_pair_close(&pair);

    return EXIT_SUCCESS;
}
