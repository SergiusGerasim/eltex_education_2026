#include "broker.h"
#include "message.h"
#include "subscriber.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#define BROKER_SHUTDOWN_TIMEOUT_SECONDS 5

/*
sig_atomic_t - целочисленный тип из <signal.h>, с которым обработчик сигнала может работать атомарно
volatile сообщает компилятору:
Значение этой переменной может измениться в момент, который не виден обычному потоку выполнения. Каждый раз читай её из памяти.
*/
static volatile sig_atomic_t broker_should_stop = 0;


static void handle_sigint(int signal_number) {
    (void)signal_number;
    broker_should_stop = 1;
}

static int parse_message(message_t *message, parsed_message_t *parsed_message) {
    char *payload = strchr(message->text, '\n');

    if (payload != NULL) {
        *payload = '\0';
        payload++;
    }

    char *save_pointer = NULL;
    char *command = strtok_r(message->text, ",", &save_pointer);
    char *pid_text = strtok_r(NULL, ",", &save_pointer);
    char *topic = strtok_r(NULL, ",", &save_pointer);
    char *extra = strtok_r(NULL, ",", &save_pointer);

    if (command == NULL || pid_text == NULL || topic == NULL || extra != NULL) {
        fprintf(stderr, "Invalid message header\n");
        return EXIT_FAILURE;
    }

    char *pid_end = NULL;
    errno = 0;
    long sender_pid = strtol(pid_text, &pid_end, 10);

    if (errno == ERANGE || pid_end == pid_text || *pid_end != '\0' || sender_pid <= 0) {
        fprintf(stderr, "Invalid PID: %s\n", pid_text);
        return EXIT_FAILURE;
    }

    parsed_message->command = command;
    parsed_message->sender_pid = sender_pid;
    parsed_message->topic = topic;
    parsed_message->payload = payload;
    return EXIT_SUCCESS;
}

static int register_publisher(publisher_node_t **publishers, long pid) {
    for (publisher_node_t *current = *publishers; current != NULL; current = current->next) {
        if (current->pid == pid) return EXIT_SUCCESS;
    }

    publisher_node_t *new_publisher = malloc(sizeof(*new_publisher));

    if (new_publisher == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    new_publisher->pid = pid;
    new_publisher->next = *publishers;
    *publishers = new_publisher;

    return EXIT_SUCCESS;
}

static void free_publishers(publisher_node_t *publishers) {
    while (publishers != NULL) {
        publisher_node_t *next = publishers->next;
        free(publishers);
        publishers = next;
    }
}

static void notify_publishers(publisher_node_t *publishers) {
    for (publisher_node_t *current = publishers; current != NULL; current = current->next) {
        if (kill((pid_t)current->pid, SIGINT) == -1) {
            if (errno == ESRCH) continue;

            perror("kill publisher");
        }
    }
}

static int register_subscription(subscription_node_t **subscriptions, long subscriber_pid, const char *topic) {
    for (subscription_node_t *current = *subscriptions; current != NULL; current = current->next) {
        if (current->subscriber_pid == subscriber_pid && strcmp(current->topic, topic) == 0) return EXIT_SUCCESS;
    }

    subscription_node_t *new_subscription = malloc(sizeof(*new_subscription));

    if (new_subscription == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    new_subscription->topic = strdup(topic);

    if (new_subscription->topic == NULL) {
        perror("strdup");
        free(new_subscription);
        return EXIT_FAILURE;
    }

    new_subscription->subscriber_pid = subscriber_pid;
    new_subscription->next = *subscriptions;
    *subscriptions = new_subscription;
    return EXIT_SUCCESS;
}

static void unregister_subscription(subscription_node_t **subscriptions, long subscriber_pid, const char *topic) {
    subscription_node_t **current = subscriptions;

    while (*current != NULL) {
        subscription_node_t *subscription = *current;

        if (subscription->subscriber_pid == subscriber_pid && strcmp(subscription->topic, topic) == 0) {
            *current = subscription->next;
            free(subscription->topic);
            free(subscription);
            return;
        }

        current = &subscription->next;
    }
}

static void free_subscriptions(subscription_node_t *subscriptions) {
    while (subscriptions != NULL) {
        subscription_node_t *next = subscriptions->next;
        free(subscriptions->topic);
        free(subscriptions);
        subscriptions = next;
    }
}

static void notify_subscribers(const subscription_node_t *subscriptions) {
    for (const subscription_node_t *current = subscriptions; current != NULL; current = current->next) {
        int already_notified = 0;

        for (const subscription_node_t *previous = subscriptions; previous != current; previous = previous->next) {
            if (previous->subscriber_pid == current->subscriber_pid) {
                already_notified = 1;
                break;
            }
        }

        if (already_notified) continue;

        if (kill((pid_t)current->subscriber_pid, SIGINT) == -1) {
            if (errno == ESRCH) continue;

            perror("kill subscriber");
        }
    }
}

static int drain_message_queue(int message_queue_id) {
    for (int elapsed_seconds = 0; elapsed_seconds < BROKER_SHUTDOWN_TIMEOUT_SECONDS; elapsed_seconds++) {
        for (;;) {
            message_t message;
            ssize_t received_size = msgrcv(message_queue_id, &message, sizeof(message.text), 0, IPC_NOWAIT | MSG_NOERROR);

            if (received_size >= 0) continue;
            if (errno == EINTR) continue;
            if (errno == ENOMSG) break;

            if (errno == EIDRM || errno == EINVAL) {
                printf("Message queue is no longer available\n");
                return EXIT_SUCCESS;
            }

            perror("msgrcv shutdown");
            return EXIT_FAILURE;
        }

        sleep(1);

        struct msqid_ds queue_info;

        if (msgctl(message_queue_id, IPC_STAT, &queue_info) == -1) {
            if (errno == EIDRM || errno == EINVAL) return EXIT_SUCCESS;

            perror("msgctl IPC_STAT");
            return EXIT_FAILURE;
        }

        if (queue_info.msg_qnum == 0) {
            printf("Message queue is empty\n");
            return EXIT_SUCCESS;
        }
    }

    fprintf(stderr, "Message queue shutdown timeout expired\n");
    return EXIT_SUCCESS;
}

static int forward_publication(int message_queue_id, const subscription_node_t *subscriptions, const parsed_message_t *publication) {
    for (const subscription_node_t *current = subscriptions; current != NULL; current = current->next) {
        if (strcmp(current->topic, publication->topic) != 0) continue;

        message_t outgoing_message = {.priority = current->subscriber_pid};
        int written = snprintf(outgoing_message.text, sizeof(outgoing_message.text), "send,%ld,%s\n%s", publication->sender_pid,
                               publication->topic, publication->payload);

        if (written < 0 || (size_t)written >= sizeof(outgoing_message.text)) {
            fprintf(stderr, "Outgoing message is too long\n");
            return EXIT_FAILURE;
        }

        if (msgsnd(message_queue_id, &outgoing_message, (size_t)written + 1, 0) == -1) {
            perror("msgsnd subscriber");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int run_broker(void) {
    struct sigaction action = {0};
    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);
    // Ctrl+C -> SIGINT
    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }


    int message_queue_id = msgget(MESSAGE_QUEUE_KEY, IPC_CREAT | IPC_EXCL | 0600);
    if (message_queue_id == -1){
        perror("msgget");
        return EXIT_FAILURE;
    }
    printf(
        "Broker started, PID: %ld, queue ID: %d\n",
        (long)getpid(),
        message_queue_id
    );

    publisher_node_t *publishers = NULL;
    subscription_node_t *subscriptions = NULL;

    while (!broker_should_stop) {
        message_t message;

        ssize_t received_size = msgrcv(message_queue_id,&message,
            sizeof(message_t) - sizeof(long),BROKER_MESSAGE_TYPE,0);
        
        if (received_size == -1) {
            if (errno == EINTR && broker_should_stop) break;

            perror("msgrcv");
            break;
        }

        if (received_size >= MESSAGE_TEXT_SIZE) {
            message.text[MESSAGE_TEXT_SIZE - 1] = '\0';
        } else {
            message.text[received_size] = '\0';
        }

        parsed_message_t parsed_message;
        if (parse_message(&message, &parsed_message) != EXIT_SUCCESS) continue;

        if (strcmp(parsed_message.command, "send") == 0) {
            if (parsed_message.payload == NULL) {
                fprintf(stderr, "Invalid send message: payload is missing\n");
                continue;
            }

            if (register_publisher(&publishers, parsed_message.sender_pid) != EXIT_SUCCESS) continue;

            printf("Publication: pid=%ld, topic=%s, payload=%s\n", parsed_message.sender_pid, parsed_message.topic, parsed_message.payload);

            if (forward_publication(message_queue_id, subscriptions, &parsed_message) != EXIT_SUCCESS) {
                if (errno == EINTR && broker_should_stop) break;
            }
        } else if (strcmp(parsed_message.command, "subscribe") == 0) {
            if (register_subscription(&subscriptions, parsed_message.sender_pid, parsed_message.topic) != EXIT_SUCCESS) continue;

            printf("Subscriber %ld subscribed to %s\n", parsed_message.sender_pid, parsed_message.topic);
        } else if (strcmp(parsed_message.command, "unsubscribe") == 0) {
            unregister_subscription(&subscriptions, parsed_message.sender_pid, parsed_message.topic);
            printf("Subscriber %ld unsubscribed from %s\n", parsed_message.sender_pid, parsed_message.topic);
        } else {
            fprintf(stderr, "Unsupported command: %s\n", parsed_message.command);
        }
    }

    notify_publishers(publishers);
    notify_subscribers(subscriptions);
    int shutdown_result = drain_message_queue(message_queue_id);

    free_publishers(publishers);
    free_subscriptions(subscriptions);

    if (msgctl(message_queue_id, IPC_RMID, NULL) == -1){
        perror("msgctl");
        return EXIT_FAILURE;
    }
    printf("Message queue removed\n");

    return shutdown_result;
}
