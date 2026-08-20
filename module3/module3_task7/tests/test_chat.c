#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEST_PATH_MAX 4096U
#define TEST_FILE_SIZE 40000U

typedef struct {
    pid_t process;
    int input;
} client_process_t;

static void delay_milliseconds(long milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = milliseconds % 1000 * 1000000L
    };
    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {}
}

static int find_free_port(uint16_t *port) {
    const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1) return 0;

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    socklen_t address_size = sizeof(address);
    int result = bind(descriptor, (const struct sockaddr *)&address, sizeof(address)) != -1;
    if (result) result = getsockname(descriptor, (struct sockaddr *)&address, &address_size) != -1;
    if (result) *port = ntohs(address.sin_port);
    close(descriptor);
    return result && *port != 0;
}

static int write_all(int descriptor, const void *data, size_t size) {
    const unsigned char *bytes = data;
    size_t offset = 0;
    while (offset < size) {
        const ssize_t written = write(descriptor, bytes + offset, size - offset);
        if (written > 0) offset += (size_t)written;
        else if (written == -1 && errno != EINTR) return 0;
    }
    return 1;
}

static pid_t start_server(const char *server_path, const char *working_directory, const char *port) {
    const pid_t process = fork();
    if (process != 0) return process;
    if (chdir(working_directory) == -1) _exit(126);
    const int output = open("server.log", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (output == -1 || dup2(output, STDOUT_FILENO) == -1 || dup2(output, STDERR_FILENO) == -1) _exit(126);
    close(output);
    execl(server_path, server_path, "127.0.0.1", port, (char *)NULL);
    _exit(127);
}

static client_process_t start_client(const char *client_path, const char *working_directory, const char *name, const char *port, const char *log_name) {
    int input_pipe[2];
    client_process_t client = {.process = -1, .input = -1};
    if (pipe(input_pipe) == -1) return client;

    client.process = fork();
    if (client.process == -1) {
        close(input_pipe[0]);
        close(input_pipe[1]);
        return client;
    }
    if (client.process == 0) {
        close(input_pipe[1]);
        if (chdir(working_directory) == -1 || dup2(input_pipe[0], STDIN_FILENO) == -1) _exit(126);
        close(input_pipe[0]);
        const int output = open(log_name, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (output == -1 || dup2(output, STDOUT_FILENO) == -1 || dup2(output, STDERR_FILENO) == -1) _exit(126);
        close(output);
        execl(client_path, client_path, name, "127.0.0.1", port, (char *)NULL);
        _exit(127);
    }

    close(input_pipe[0]);
    client.input = input_pipe[1];
    return client;
}

static int wait_success(pid_t process, long timeout_milliseconds) {
    for (long elapsed = 0; elapsed < timeout_milliseconds; elapsed += 10) {
        int status;
        const pid_t result = waitpid(process, &status, WNOHANG);
        if (result == process) return WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS;
        if (result == -1) return 0;
        delay_milliseconds(10);
    }
    kill(process, SIGKILL);
    waitpid(process, NULL, 0);
    return 0;
}

static int create_payload(const char *path) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) return 0;
    for (size_t index = 0; index < TEST_FILE_SIZE; ++index) {
        const unsigned char byte = (unsigned char)(index * 37U);
        if (fwrite(&byte, 1, 1, file) != 1) {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) != EOF;
}

static int file_contains(const char *path, const char *needle) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return 0;
    char buffer[8192];
    const size_t size = fread(buffer, 1, sizeof(buffer) - 1U, file);
    fclose(file);
    buffer[size] = '\0';
    return strstr(buffer, needle) != NULL;
}

static int received_payload_matches(const char *directory) {
    char downloads[TEST_PATH_MAX];
    snprintf(downloads, sizeof(downloads), "%s/downloads", directory);
    DIR *stream = opendir(downloads);
    if (stream == NULL) return 0;

    int result = 0;
    struct dirent *entry;
    while ((entry = readdir(stream)) != NULL) {
        const size_t name_size = strlen(entry->d_name);
        static const char suffix[] = "_payload.bin";
        if (name_size < sizeof(suffix) - 1U || strcmp(entry->d_name + name_size - (sizeof(suffix) - 1U), suffix) != 0) continue;

        char path[TEST_PATH_MAX];
        const size_t downloads_size = strlen(downloads);
        if (downloads_size + 1U + name_size + 1U > sizeof(path)) continue;
        memcpy(path, downloads, downloads_size);
        path[downloads_size] = '/';
        memcpy(path + downloads_size + 1U, entry->d_name, name_size + 1U);
        FILE *file = fopen(path, "rb");
        if (file == NULL) continue;

        result = 1;
        for (size_t index = 0; index < TEST_FILE_SIZE; ++index) {
            const int byte = fgetc(file);
            if (byte != (unsigned char)(index * 37U)) {
                result = 0;
                break;
            }
        }
        if (result && fgetc(file) != EOF) result = 0;
        fclose(file);
        break;
    }
    closedir(stream);
    return result;
}

static int executable_path(const char *argument, char *path, size_t capacity) {
    if (argument[0] == '/') {
        const size_t size = strlen(argument);
        if (size + 1U > capacity) return 0;
        memcpy(path, argument, size + 1U);
        return 1;
    }

    if (getcwd(path, capacity) == NULL) return 0;
    const size_t directory_size = strlen(path);
    const size_t argument_size = strlen(argument);
    if (directory_size + 1U + argument_size + 1U > capacity) return 0;
    path[directory_size] = '/';
    memcpy(path + directory_size + 1U, argument, argument_size + 1U);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) return EXIT_FAILURE;

    char server_path[TEST_PATH_MAX];
    char client_path[TEST_PATH_MAX];
    if (!executable_path(argv[1], server_path, sizeof(server_path)) || !executable_path(argv[2], client_path, sizeof(client_path))) return EXIT_FAILURE;

    char temporary_directory[] = "/tmp/module3_task7_chat_XXXXXX";
    if (mkdtemp(temporary_directory) == NULL) return EXIT_FAILURE;

    char payload_path[TEST_PATH_MAX];
    snprintf(payload_path, sizeof(payload_path), "%s/payload.bin", temporary_directory);
    if (!create_payload(payload_path)) return EXIT_FAILURE;

    uint16_t port;
    if (!find_free_port(&port)) return EXIT_FAILURE;
    char port_text[6];
    snprintf(port_text, sizeof(port_text), "%u", port);

    const pid_t server = start_server(server_path, temporary_directory, port_text);
    if (server == -1) return EXIT_FAILURE;
    delay_milliseconds(100);

    client_process_t bob = start_client(client_path, temporary_directory, "Bob", port_text, "bob.log");
    if (bob.process == -1) return EXIT_FAILURE;
    delay_milliseconds(100);
    client_process_t alice = start_client(client_path, temporary_directory, "Alice", port_text, "alice.log");
    if (alice.process == -1) return EXIT_FAILURE;
    delay_milliseconds(100);

    char commands[TEST_PATH_MAX + 64U];
    const int command_size = snprintf(commands, sizeof(commands), "Hello, Bob\n/file %s\n", payload_path);
    int success = command_size > 0 && (size_t)command_size < sizeof(commands) && write_all(alice.input, commands, (size_t)command_size);
    delay_milliseconds(1000);
    if (success) success = write_all(alice.input, "/exit\n", 6);
    close(alice.input);
    if (success) success = wait_success(alice.process, 3000);

    delay_milliseconds(200);
    if (success) success = write_all(bob.input, "/exit\n", 6);
    close(bob.input);
    if (success) success = wait_success(bob.process, 3000);

    kill(server, SIGINT);
    if (success) success = wait_success(server, 3000);

    char bob_log[TEST_PATH_MAX];
    snprintf(bob_log, sizeof(bob_log), "%s/bob.log", temporary_directory);
    if (success) success = file_contains(bob_log, "Alice: Hello, Bob");
    if (success) success = received_payload_matches(temporary_directory);

    if (!success) {
        fprintf(stderr, "Full chat integration test failed; artifacts: %s\n", temporary_directory);
        return EXIT_FAILURE;
    }

    puts("Full chat integration tests passed");
    return EXIT_SUCCESS;
}
