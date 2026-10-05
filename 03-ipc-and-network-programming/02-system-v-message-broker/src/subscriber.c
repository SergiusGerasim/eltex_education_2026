#include "subscriber.h"
#include "message.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t subscriber_should_stop = 0;

static void handle_sigint(int signal_number) {
    (void)signal_number;
    subscriber_should_stop = 1;
}

static int install_signal_handler(void) {
    struct sigaction action = {0};
    action.sa_handler = handle_sigint;

    if (sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int send_subscription_command(int message_queue_id, const char *command, pid_t subscriber_pid, const char *topic) {
    message_t message = {.priority = BROKER_MESSAGE_TYPE};
    int written = snprintf(message.text, sizeof(message.text), "%s,%ld,%s", command, (long)subscriber_pid, topic);

    if (written < 0 || (size_t)written >= sizeof(message.text)) {
        fprintf(stderr, "Subscription command is too long\n");
        return EXIT_FAILURE;
    }

    if (msgsnd(message_queue_id, &message, (size_t)written + 1, 0) == -1) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int run_subscriber(int topic_count, char *const topics[]) {
    if (install_signal_handler() != EXIT_SUCCESS) return EXIT_FAILURE;

    int message_queue_id = msgget(MESSAGE_QUEUE_KEY, 0);

    if (message_queue_id == -1) {
        perror("msgget");
        return EXIT_FAILURE;
    }

    pid_t subscriber_pid = getpid();
    int subscribed_count = 0;
    int result = EXIT_SUCCESS;
    int queue_available = 1;

    for (int i = 0; i < topic_count; i++) {
        if (send_subscription_command(message_queue_id, "subscribe", subscriber_pid, topics[i]) != EXIT_SUCCESS) {
            perror("msgsnd subscribe");
            result = EXIT_FAILURE;
            goto unsubscribe;
        }

        subscribed_count++;
        printf("Subscribed to %s\n", topics[i]);
    }

    printf("Subscriber started: pid=%ld\n", (long)subscriber_pid);

    while (!subscriber_should_stop) {
        message_t message;
        ssize_t received_size = msgrcv(message_queue_id, &message, sizeof(message.text), (long)subscriber_pid, 0);

        if (received_size == -1) {
            if (errno == EINTR && subscriber_should_stop) break;

            if (errno == EIDRM || errno == EINVAL) {
                fprintf(stderr, "Message queue is no longer available\n");
                queue_available = 0;
                break;
            }

            perror("msgrcv");
            result = EXIT_FAILURE;
            break;
        }

        if (received_size >= MESSAGE_TEXT_SIZE) {
            message.text[MESSAGE_TEXT_SIZE - 1] = '\0';
        } else {
            message.text[received_size] = '\0';
        }

        printf("Received: %s\n", message.text);
    }

    if (!queue_available) return result;

unsubscribe:
    for (int i = 0; i < subscribed_count; i++) {
        if (send_subscription_command(message_queue_id, "unsubscribe", subscriber_pid, topics[i]) == EXIT_SUCCESS) continue;
        if (errno == EIDRM || errno == EINVAL) break;

        perror("msgsnd unsubscribe");
        result = EXIT_FAILURE;
    }

    return result;
}
