#include "client.h"
#include "protocol.h"
#include "signal_handler.h"
#include "tcp_socket.h"

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#define CLIENT_OUTPUT_CAPACITY (CHAT_PACKET_MAX_SIZE * 4U)
#define CLIENT_INCOMING_FILES_MAX 16U
#define CLIENT_PATH_MAX 512U
#define DOWNLOAD_DIRECTORY "downloads"

typedef struct {
    bool active;
    uint64_t sender_id;
    uint64_t transfer_id;
    uint64_t expected_size;
    uint64_t received_size;
    FILE *file;
    char temporary_path[CLIENT_PATH_MAX];
    char final_path[CLIENT_PATH_MAX];
} incoming_file_t;

typedef struct {
    FILE *file;
    uint64_t transfer_id;
    uint64_t file_size;
    uint64_t sent_size;
    char file_name[CHAT_FILE_NAME_MAX + 1U];
} outgoing_file_t;

typedef struct {
    const client_config_t *config;
    tcp_socket_t socket;
    uint64_t sender_id;
    uint8_t input[CHAT_PACKET_MAX_SIZE];
    size_t input_size;
    uint8_t output[CLIENT_OUTPUT_CAPACITY];
    size_t output_offset;
    size_t output_size;
    outgoing_file_t outgoing_file;
    incoming_file_t incoming_files[CLIENT_INCOMING_FILES_MAX];
} client_state_t;

static bool generate_id(uint64_t *identifier) {
    do {
        size_t offset = 0;
        while (offset < sizeof(*identifier)) {
            const ssize_t received = getrandom((unsigned char *)identifier + offset, sizeof(*identifier) - offset, 0);
            if (received > 0) offset += (size_t)received;
            else if (received == -1 && errno != EINTR) return false;
            else if (received == 0) {
                errno = EIO;
                return false;
            }
        }
    } while (*identifier == 0);
    return true;
}

static void initialize_message(const client_state_t *client, chat_message_t *message, chat_packet_type_t type) {
    memset(message, 0, sizeof(*message));
    message->type = type;
    message->sender_id = client->sender_id;
    memcpy(message->sender_name, client->config->name, strlen(client->config->name) + 1U);
}

static size_t pending_output_size(const client_state_t *client) {
    return client->output_size - client->output_offset;
}

static bool queue_message(client_state_t *client, const chat_message_t *message) {
    uint8_t frame[CHAT_PACKET_MAX_SIZE];
    size_t frame_size;
    if (!protocol_encode(message, frame, sizeof(frame), &frame_size)) return false;

    const size_t pending_size = pending_output_size(client);
    if (client->output_offset != 0 && pending_size != 0) memmove(client->output, client->output + client->output_offset, pending_size);
    client->output_offset = 0;
    client->output_size = pending_size;
    if (frame_size > sizeof(client->output) - client->output_size) {
        errno = ENOBUFS;
        return false;
    }

    memcpy(client->output + client->output_size, frame, frame_size);
    client->output_size += frame_size;
    return true;
}

static bool flush_output(client_state_t *client) {
    while (client->output_offset < client->output_size) {
        const ssize_t sent = tcp_socket_send(&client->socket, client->output + client->output_offset, client->output_size - client->output_offset);
        if (sent > 0) client->output_offset += (size_t)sent;
        else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        else return false;
    }

    client->output_offset = 0;
    client->output_size = 0;
    return true;
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static bool safe_file_name(const char *name) {
    if (*name == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return false;
    return strchr(name, '/') == NULL && strchr(name, '\\') == NULL;
}

static void close_incoming_file(incoming_file_t *incoming, bool keep_file) {
    if (incoming->file != NULL) fclose(incoming->file);
    if (!keep_file && incoming->temporary_path[0] != '\0') unlink(incoming->temporary_path);
    memset(incoming, 0, sizeof(*incoming));
}

static incoming_file_t *find_incoming_file(client_state_t *client, uint64_t sender_id, uint64_t transfer_id) {
    for (size_t index = 0; index < CLIENT_INCOMING_FILES_MAX; ++index) {
        incoming_file_t *incoming = &client->incoming_files[index];
        if (incoming->active && incoming->sender_id == sender_id && incoming->transfer_id == transfer_id) return incoming;
    }
    return NULL;
}

static incoming_file_t *free_incoming_file(client_state_t *client) {
    for (size_t index = 0; index < CLIENT_INCOMING_FILES_MAX; ++index) {
        if (!client->incoming_files[index].active) return &client->incoming_files[index];
    }
    return NULL;
}

static void cancel_incoming_files(client_state_t *client, uint64_t sender_id) {
    for (size_t index = 0; index < CLIENT_INCOMING_FILES_MAX; ++index) {
        incoming_file_t *incoming = &client->incoming_files[index];
        if (incoming->active && incoming->sender_id == sender_id) close_incoming_file(incoming, false);
    }
}

static bool begin_incoming_file(client_state_t *client, const chat_message_t *message) {
    if (!safe_file_name(message->file_name)) return false;
    if (find_incoming_file(client, message->sender_id, message->transfer_id) != NULL) return false;
    if (mkdir(DOWNLOAD_DIRECTORY, 0700) == -1 && errno != EEXIST) return false;

    incoming_file_t *incoming = free_incoming_file(client);
    if (incoming == NULL) return false;
    memset(incoming, 0, sizeof(*incoming));

    const int final_length = snprintf(incoming->final_path, sizeof(incoming->final_path), "%s/%016llx_%016llx_%s", DOWNLOAD_DIRECTORY,
                                      (unsigned long long)message->sender_id, (unsigned long long)message->transfer_id, message->file_name);
    if (final_length < 0 || (size_t)final_length >= sizeof(incoming->final_path)) return false;
    const int temporary_length = snprintf(incoming->temporary_path, sizeof(incoming->temporary_path), "%s.part", incoming->final_path);
    if (temporary_length < 0 || (size_t)temporary_length >= sizeof(incoming->temporary_path)) return false;

    incoming->file = fopen(incoming->temporary_path, "wb");
    if (incoming->file == NULL) return false;
    incoming->active = true;
    incoming->sender_id = message->sender_id;
    incoming->transfer_id = message->transfer_id;
    incoming->expected_size = message->file_size;
    printf("Receiving file from %s: %s (%llu bytes)\n", message->sender_name, message->file_name, (unsigned long long)message->file_size);
    return true;
}

static bool write_incoming_chunk(client_state_t *client, const chat_message_t *message) {
    incoming_file_t *incoming = find_incoming_file(client, message->sender_id, message->transfer_id);
    if (incoming == NULL || message->file_data_size > incoming->expected_size - incoming->received_size) return false;
    if (fwrite(message->file_data, 1, message->file_data_size, incoming->file) != message->file_data_size) {
        close_incoming_file(incoming, false);
        return false;
    }
    incoming->received_size += message->file_data_size;
    return true;
}

static bool finish_incoming_file(client_state_t *client, const chat_message_t *message) {
    incoming_file_t *incoming = find_incoming_file(client, message->sender_id, message->transfer_id);
    if (incoming == NULL || incoming->received_size != incoming->expected_size) return false;
    if (fclose(incoming->file) == EOF) {
        incoming->file = NULL;
        close_incoming_file(incoming, false);
        return false;
    }
    incoming->file = NULL;
    if (rename(incoming->temporary_path, incoming->final_path) == -1) {
        close_incoming_file(incoming, false);
        return false;
    }
    printf("File saved: %s\n", incoming->final_path);
    close_incoming_file(incoming, true);
    return true;
}

static bool handle_received_message(client_state_t *client, const chat_message_t *message) {
    if (message->sender_id == client->sender_id) return true;
    if (message->type == CHAT_PACKET_JOIN) printf("%s joined the chat\n", message->sender_name);
    else if (message->type == CHAT_PACKET_MESSAGE) printf("%s: %s\n", message->sender_name, message->text);
    else if (message->type == CHAT_PACKET_LEAVE) {
        cancel_incoming_files(client, message->sender_id);
        printf("%s left the chat\n", message->sender_name);
    }
    else if (message->type == CHAT_PACKET_FILE_BEGIN) return begin_incoming_file(client, message);
    else if (message->type == CHAT_PACKET_FILE_CHUNK) return write_incoming_chunk(client, message);
    else if (message->type == CHAT_PACKET_FILE_END) return finish_incoming_file(client, message);
    return true;
}

static bool process_input(client_state_t *client) {
    while (client->input_size != 0) {
        size_t frame_size;
        const protocol_frame_status_t status = protocol_frame_size(client->input, client->input_size, &frame_size);
        if (status == PROTOCOL_FRAME_INVALID) return false;
        if (status == PROTOCOL_FRAME_INCOMPLETE) return true;

        chat_message_t message;
        if (!protocol_decode(client->input, frame_size, &message) || !handle_received_message(client, &message)) return false;
        const size_t remaining_size = client->input_size - frame_size;
        memmove(client->input, client->input + frame_size, remaining_size);
        client->input_size = remaining_size;
    }
    return true;
}

static bool receive_messages(client_state_t *client) {
    for (;;) {
        if (client->input_size == sizeof(client->input)) return false;
        const ssize_t received = tcp_socket_receive(&client->socket, client->input + client->input_size, sizeof(client->input) - client->input_size);
        if (received > 0) {
            client->input_size += (size_t)received;
            if (!process_input(client)) return false;
        } else if (received == 0) {
            fprintf(stderr, "Server closed the connection\n");
            return false;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        } else {
            return false;
        }
    }
}

static void close_outgoing_file(outgoing_file_t *outgoing) {
    if (outgoing->file != NULL) fclose(outgoing->file);
    memset(outgoing, 0, sizeof(*outgoing));
}

static bool start_outgoing_file(client_state_t *client, const char *path) {
    if (client->outgoing_file.file != NULL) {
        fprintf(stderr, "A file is already being sent\n");
        return true;
    }

    const char *name = base_name(path);
    const size_t name_size = strlen(name);
    if (!safe_file_name(name) || name_size > CHAT_FILE_NAME_MAX) {
        fprintf(stderr, "Invalid file name\n");
        return true;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror("fopen");
        return true;
    }
    if (fseeko(file, 0, SEEK_END) == -1) {
        perror("fseeko");
        fclose(file);
        return true;
    }
    const off_t size = ftello(file);
    if (size < 0 || fseeko(file, 0, SEEK_SET) == -1) {
        perror("file size");
        fclose(file);
        return true;
    }

    outgoing_file_t *outgoing = &client->outgoing_file;
    outgoing->file = file;
    outgoing->file_size = (uint64_t)size;
    memcpy(outgoing->file_name, name, name_size + 1U);
    if (!generate_id(&outgoing->transfer_id)) {
        perror("getrandom");
        close_outgoing_file(outgoing);
        return false;
    }

    chat_message_t begin;
    initialize_message(client, &begin, CHAT_PACKET_FILE_BEGIN);
    begin.transfer_id = outgoing->transfer_id;
    begin.file_size = outgoing->file_size;
    memcpy(begin.file_name, outgoing->file_name, name_size + 1U);
    if (!queue_message(client, &begin)) {
        close_outgoing_file(outgoing);
        return false;
    }
    printf("Sending file: %s (%llu bytes)\n", outgoing->file_name, (unsigned long long)outgoing->file_size);
    return true;
}

static bool pump_outgoing_file(client_state_t *client) {
    outgoing_file_t *outgoing = &client->outgoing_file;
    if (outgoing->file == NULL || sizeof(client->output) - pending_output_size(client) < CHAT_PACKET_MAX_SIZE) return true;

    if (outgoing->sent_size < outgoing->file_size) {
        chat_message_t chunk;
        initialize_message(client, &chunk, CHAT_PACKET_FILE_CHUNK);
        chunk.transfer_id = outgoing->transfer_id;
        const uint64_t remaining = outgoing->file_size - outgoing->sent_size;
        chunk.file_data_size = remaining < CHAT_FILE_CHUNK_MAX ? (size_t)remaining : CHAT_FILE_CHUNK_MAX;
        const size_t read_size = fread(chunk.file_data, 1, chunk.file_data_size, outgoing->file);
        if (read_size != chunk.file_data_size) {
            perror("fread");
            close_outgoing_file(outgoing);
            return false;
        }
        if (!queue_message(client, &chunk)) return false;
        outgoing->sent_size += read_size;
        return true;
    }

    chat_message_t end;
    initialize_message(client, &end, CHAT_PACKET_FILE_END);
    end.transfer_id = outgoing->transfer_id;
    if (!queue_message(client, &end)) return false;
    printf("File queued completely: %s\n", outgoing->file_name);
    close_outgoing_file(outgoing);
    return true;
}

static bool handle_stdin(client_state_t *client, bool *running) {
    char input[CHAT_TEXT_MAX + 2U];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        if (feof(stdin)) {
            *running = false;
            return true;
        }
        if (errno == EINTR) {
            clearerr(stdin);
            return true;
        }
        return false;
    }

    size_t input_size = strlen(input);
    const bool has_newline = input_size != 0 && input[input_size - 1U] == '\n';
    if (has_newline) input[--input_size] = '\0';
    if (!has_newline && input_size > CHAT_TEXT_MAX) {
        int character;
        do {
            character = getchar();
        } while (character != '\n' && character != EOF);
        fprintf(stderr, "Input is too long\n");
        return true;
    }
    if (input_size == 0) return true;
    if (strcmp(input, "/exit") == 0) {
        *running = false;
        return true;
    }
    if (strncmp(input, "/file ", 6) == 0) return start_outgoing_file(client, input + 6);

    chat_message_t message;
    initialize_message(client, &message, CHAT_PACKET_MESSAGE);
    memcpy(message.text, input, input_size + 1U);
    return queue_message(client, &message);
}

static void drain_output(client_state_t *client) {
    while (pending_output_size(client) != 0) {
        if (!flush_output(client)) return;
        if (pending_output_size(client) == 0) return;
        struct pollfd event = {.fd = client->socket.descriptor, .events = POLLOUT};
        const int status = poll(&event, 1, 1000);
        if (status <= 0) return;
    }
}

static void cleanup_client(client_state_t *client) {
    close_outgoing_file(&client->outgoing_file);
    for (size_t index = 0; index < CLIENT_INCOMING_FILES_MAX; ++index) close_incoming_file(&client->incoming_files[index], false);
    tcp_socket_close(&client->socket);
}

int client_run(const client_config_t *config) {
    if (config == NULL || config->name == NULL || config->server_address == NULL || config->port == 0) return EXIT_FAILURE;
    if (signal_handler_install() == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    client_state_t *client = calloc(1, sizeof(*client));
    if (client == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }
    client->config = config;
    tcp_socket_init(&client->socket);
    if (!generate_id(&client->sender_id)) {
        perror("getrandom");
        free(client);
        return EXIT_FAILURE;
    }
    if (!tcp_socket_connect(&client->socket, config->server_address, config->port)) {
        perror("tcp_socket_connect");
        free(client);
        return EXIT_FAILURE;
    }

    chat_message_t join;
    initialize_message(client, &join, CHAT_PACKET_JOIN);
    if (!queue_message(client, &join)) {
        cleanup_client(client);
        free(client);
        return EXIT_FAILURE;
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);
    bool running = true;
    int result = EXIT_SUCCESS;

    while (running && signal_handler_stop_requested() == 0) {
        if (!pump_outgoing_file(client)) {
            result = EXIT_FAILURE;
            break;
        }

        struct pollfd events[2] = {
            {.fd = client->socket.descriptor, .events = POLLIN | (pending_output_size(client) != 0 ? POLLOUT : 0)},
            {.fd = STDIN_FILENO, .events = POLLIN}
        };
        const int status = poll(events, 2, -1);
        if (status == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            result = EXIT_FAILURE;
            break;
        }

        if ((events[0].revents & POLLIN) != 0 && !receive_messages(client)) {
            result = EXIT_FAILURE;
            break;
        }
        if ((events[0].revents & POLLOUT) != 0 && !flush_output(client)) {
            result = EXIT_FAILURE;
            break;
        }
        if ((events[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            fprintf(stderr, "Server connection error\n");
            result = EXIT_FAILURE;
            break;
        }
        if ((events[1].revents & POLLIN) != 0 && !handle_stdin(client, &running)) {
            result = EXIT_FAILURE;
            break;
        }
        if ((events[1].revents & (POLLHUP | POLLNVAL)) != 0) running = false;
    }

    if (client->socket.descriptor != -1) {
        chat_message_t leave;
        initialize_message(client, &leave, CHAT_PACKET_LEAVE);
        if (queue_message(client, &leave)) drain_output(client);
    }
    cleanup_client(client);
    free(client);
    return result;
}
