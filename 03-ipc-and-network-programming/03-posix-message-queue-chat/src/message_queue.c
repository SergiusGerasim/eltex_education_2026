#include "message_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#define QUEUE_OPEN_RETRY_COUNT 50
#define QUEUE_OPEN_RETRY_DELAY_NS 20000000L

static mqd_t open_existing_queue(const char *name, int flags) {
    const struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = QUEUE_OPEN_RETRY_DELAY_NS
    };

    for (int attempt = 0; attempt < QUEUE_OPEN_RETRY_COUNT; ++attempt) {
        mqd_t queue = mq_open(name, flags);

        if (queue != (mqd_t)-1) return queue;
        if (errno != ENOENT) return (mqd_t)-1;

        nanosleep(&delay, NULL);
    }

    errno = ETIMEDOUT;
    return (mqd_t)-1;
}

int message_queue_pair_set_names(message_queue_pair_t *pair, const char *base_name) {
    if (pair == NULL || base_name == NULL) return -1;

    if (base_name[0] != '/' || base_name[1] == '\0' || strchr(base_name + 1, '/') != NULL) {
        fprintf(stderr, "Queue name must start with one '/' and contain at least one character\n");
        return -1;
    }
    int length = snprintf(pair->first_name, sizeof(pair->first_name), "%s_1", base_name);
    if (length < 0 || (size_t)length >= sizeof(pair->first_name)) {
        fprintf(stderr, "First queue name is too long\n");
        return -1;
    }
    length = snprintf(pair->second_name, sizeof(pair->second_name), "%s_2", base_name);
    if (length < 0 || (size_t)length >= sizeof(pair->second_name)) {
        fprintf(stderr, "Second queue name is too long\n");
        return -1;
    }


    return EXIT_SUCCESS;
}

int message_queue_pair_open(message_queue_pair_t *pair, const char *base_name) {
    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_maxmsg = CHAT_MAX_MESSAGES,
        .mq_msgsize = CHAT_MESSAGE_SIZE,
        .mq_curmsgs = 0
    };
    if (message_queue_pair_set_names(pair, base_name) == -1) return -1;
    pair->receive_queue = (mqd_t)-1;
    pair->send_queue = (mqd_t)-1;
    pair->is_owner = false;

    mqd_t first_queue = mq_open(
        pair->first_name,
        O_CREAT | O_EXCL | O_RDONLY,
        0666,
        &attributes
    );

    if (first_queue != (mqd_t)-1) {
        pair->receive_queue = first_queue;
        pair->is_owner = true;

        pair->send_queue = mq_open(
            pair->second_name,
            O_CREAT | O_EXCL | O_WRONLY,
            0666,
            &attributes
        );

        if (pair->send_queue == (mqd_t)-1) {
            perror("mq_open second queue");

            mq_close(pair->receive_queue);
            mq_unlink(pair->first_name);

            pair->receive_queue = (mqd_t)-1;
            pair->is_owner = false;
            return -1;
        }
        return 0;
    } else if (errno == EEXIST) {
        pair->send_queue = mq_open(pair->first_name, O_WRONLY);

        if (pair->send_queue == (mqd_t)-1) {
            perror("mq_open first queue");
            return -1;
        }

        pair->receive_queue = open_existing_queue(pair->second_name, O_RDONLY);

        if (pair->receive_queue == (mqd_t)-1) {
            perror("mq_open second queue");

            mq_close(pair->send_queue);
            pair->send_queue = (mqd_t)-1;

            return -1;
        }
        return 0;
    } else {
        perror("mq_open");
        return -1;
    }
}

void message_queue_pair_close(message_queue_pair_t *pair) {
    if (pair == NULL) return;

    if (pair->receive_queue != (mqd_t)-1) {
        if (mq_close(pair->receive_queue) == -1) perror("mq_close receive queue");
        pair->receive_queue = (mqd_t)-1;
    }

    if (pair->send_queue != (mqd_t)-1) {
        if (mq_close(pair->send_queue) == -1) perror("mq_close send queue");
        pair->send_queue = (mqd_t)-1;
    }

    if (pair->is_owner) {
        if (mq_unlink(pair->first_name) == -1) perror("mq_unlink first queue");
        if (mq_unlink(pair->second_name) == -1) perror("mq_unlink second queue");
    }
    pair->is_owner = false;
}

int message_queue_send(mqd_t queue, const char *message, unsigned int priority) {
    if (queue == (mqd_t)-1 || message == NULL) return -1;

    size_t message_size = strlen(message) + 1;

    if (message_size > CHAT_MESSAGE_SIZE) {
        fprintf(stderr, "Message is too long\n");
        return -1;
    }

    if (mq_send(queue, message, message_size, priority) == -1) {
        perror("mq_send");
        return -1;
    }

    return 0;
}

ssize_t message_queue_receive(mqd_t queue, char *buffer, size_t buffer_size, unsigned int *priority){
    if (queue == (mqd_t)-1 || buffer == NULL || priority == NULL) return -1;

    if (buffer_size < CHAT_MESSAGE_SIZE){
        fprintf(stderr, "Receive buffer is too small\n");
        return -1;
    }

    ssize_t received = mq_receive(queue, buffer, buffer_size, priority);
    if (received == -1){
        perror("mq_receive");
        return -1;
    }
    return received;
}

ssize_t message_queue_receive_timed(mqd_t queue, char *buffer, size_t buffer_size, unsigned int *priority, long timeout_ms) {
    if (queue == (mqd_t)-1 || buffer == NULL || priority == NULL || timeout_ms < 0) return -1;

    if (buffer_size < CHAT_MESSAGE_SIZE) {
        fprintf(stderr, "Receive buffer is too small\n");
        return -1;
    }

    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) == -1) return -1;

    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (timeout_ms % 1000) * 1000000L;

    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    return mq_timedreceive(queue, buffer, buffer_size, priority, &deadline);
}
