#include "publisher.h"
#include "message.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#define QUEUE_CHECK_INTERVAL_MS 500

static int check_queue_available(int message_queue_id) {
    struct msqid_ds queue_info;

    if (msgctl(message_queue_id, IPC_STAT, &queue_info) == 0) return 1;
    if (errno == EIDRM || errno == EINVAL) return 0;

    perror("msgctl IPC_STAT");
    return -1;
}

static void print_payload_prompt(void) {
    printf("Enter payload (Ctrl+D to finish): ");
    fflush(stdout);
}

int run_publisher(const char *topic) {
    int message_queue_id = msgget(MESSAGE_QUEUE_KEY, 0);
    if (message_queue_id == -1){
        perror("msgget");
        return EXIT_FAILURE;
    }

    if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "Failed to disable stdin buffering\n");
        return EXIT_FAILURE;
    }

    char payload[256];
    struct pollfd input = {
        .fd = STDIN_FILENO,
        .events = POLLIN
    };

    print_payload_prompt();

    for (;;) {
        input.revents = 0;
        int poll_result = poll(&input, 1, QUEUE_CHECK_INTERVAL_MS);

        if (poll_result == -1) {
            if (errno == EINTR) continue;

            perror("poll");
            return EXIT_FAILURE;
        }

        int queue_status = check_queue_available(message_queue_id);

        if (queue_status == 0) {
            fprintf(stderr, "\nMessage queue is no longer available\n");
            break;
        }

        if (queue_status == -1) return EXIT_FAILURE;
        if (poll_result == 0) continue;

        if ((input.revents & (POLLERR | POLLNVAL)) != 0) {
            fprintf(stderr, "Standard input is unavailable\n");
            return EXIT_FAILURE;
        }

        if ((input.revents & POLLIN) == 0) {
            if ((input.revents & POLLHUP) != 0) {
                printf("\nPublisher input closed\n");
                break;
            }

            continue;
        }

        if (fgets(payload, sizeof(payload), stdin) == NULL) {
            if (feof(stdin)) {
                printf("\nPublisher finished\n");
                break;
            }

            perror("fgets");
            return EXIT_FAILURE;
        }

        payload[strcspn(payload, "\n")] = '\0';

        message_t message = {.priority = BROKER_MESSAGE_TYPE};
        int written = snprintf(message.text, sizeof(message.text), "send,%ld,%s\n%s", (long)getpid(), topic, payload);

        if (written < 0 || (size_t)written >= sizeof(message.text)) {
            fprintf(stderr, "Message is too long\n");
            print_payload_prompt();
            continue;
        }

        if (msgsnd(message_queue_id, &message, (size_t)written + 1, 0) == -1) {
            if (errno == EIDRM || errno == EINVAL) {
                fprintf(stderr, "Message queue is no longer available\n");
                break;
            }

            perror("msgsnd");
            return EXIT_FAILURE;
        }

        printf("Message sent\n");
        print_payload_prompt();
    }

    return EXIT_SUCCESS;
}
