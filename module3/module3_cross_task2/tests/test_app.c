#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEST_DRIVER_COUNT 3U

typedef struct {
    pid_t process_id;
    int input;
    int output;
} app_process_t;

static bool read_line(int descriptor, char *buffer, size_t capacity) {
    size_t offset = 0;
    while (offset + 1U < capacity) {
        fd_set descriptors;
        FD_ZERO(&descriptors);
        FD_SET(descriptor, &descriptors);
        struct timeval timeout = {.tv_sec = 3};
        int status;
        do {
            status = select(descriptor + 1, &descriptors, NULL, NULL, &timeout);
        } while (status == -1 && errno == EINTR);
        if (status <= 0) return false;

        char character;
        if (read(descriptor, &character, 1) != 1) return false;
        buffer[offset++] = character;
        if (character == '\n') break;
    }
    buffer[offset] = '\0';
    return true;
}

static void write_command(int descriptor, const char *command) {
    const size_t size = strlen(command);
    assert(write(descriptor, command, size) == (ssize_t)size);
}

static app_process_t start_app(const char *program) {
    int input_pipe[2];
    int output_pipe[2];
    assert(pipe(input_pipe) == 0);
    assert(pipe(output_pipe) == 0);

    const pid_t process_id = fork();
    assert(process_id != -1);
    if (process_id == 0) {
        assert(dup2(input_pipe[0], STDIN_FILENO) != -1);
        assert(dup2(output_pipe[1], STDOUT_FILENO) != -1);
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        execl(program, program, (char *)NULL);
        _exit(EXIT_FAILURE);
    }

    close(input_pipe[0]);
    close(output_pipe[1]);
    return (app_process_t){
        .process_id = process_id,
        .input = input_pipe[1],
        .output = output_pipe[0]
    };
}

static bool wait_for_exit(pid_t process_id, int *status) {
    const struct timespec pause = {.tv_nsec = 10000000L};
    for (int attempt = 0; attempt < 300; ++attempt) {
        const pid_t result = waitpid(process_id, status, WNOHANG);
        if (result == process_id) return true;
        assert(result == 0 || (result == -1 && errno == EINTR));
        nanosleep(&pause, NULL);
    }
    return false;
}

static long create_driver(app_process_t *app) {
    char line[256];
    write_command(app->input, "create_driver\n");
    assert(read_line(app->output, line, sizeof(line)));

    long driver_pid = 0;
    assert(sscanf(line, "Driver created: %ld", &driver_pid) == 1);
    assert(driver_pid > 0);

    assert(read_line(app->output, line, sizeof(line)));
    long connected_pid = 0;
    assert(sscanf(line, "Driver connected: %ld", &connected_pid) == 1);
    assert(connected_pid == driver_pid);
    return driver_pid;
}

static void stop_app(app_process_t *app) {
    assert(kill(app->process_id, SIGTERM) == 0);
    int status = 0;
    const bool exited = wait_for_exit(app->process_id, &status);
    if (!exited) {
        kill(app->process_id, SIGKILL);
        waitpid(app->process_id, &status, 0);
    }
    assert(exited);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == EXIT_SUCCESS);
    close(app->input);
    close(app->output);
}

static void test_three_drivers_and_crash(const char *program) {
    app_process_t app = start_app(program);
    char line[256];

    write_command(app.input, "get_drivers\n");
    assert(read_line(app.output, line, sizeof(line)));
    assert(strcmp(line, "No drivers\n") == 0);

    long driver_pids[TEST_DRIVER_COUNT];
    for (size_t index = 0; index < TEST_DRIVER_COUNT; ++index) driver_pids[index] = create_driver(&app);

    write_command(app.input, "get_drivers\n");
    bool reported[TEST_DRIVER_COUNT] = {false};
    for (size_t response = 0; response < TEST_DRIVER_COUNT; ++response) {
        assert(read_line(app.output, line, sizeof(line)));
        long process_id = 0;
        char state[32];
        assert(sscanf(line, "Driver %ld: %31s", &process_id, state) == 2);
        assert(strcmp(state, "Available") == 0);
        for (size_t index = 0; index < TEST_DRIVER_COUNT; ++index) {
            if (driver_pids[index] == process_id) reported[index] = true;
        }
    }
    for (size_t index = 0; index < TEST_DRIVER_COUNT; ++index) assert(reported[index]);

    char command[128];
    const int command_size = snprintf(command, sizeof(command), "send_task %ld 2\n", driver_pids[0]);
    assert(command_size > 0 && (size_t)command_size < sizeof(command));
    write_command(app.input, command);
    assert(read_line(app.output, line, sizeof(line)));
    assert(strstr(line, ": Task accepted 2\n") != NULL);

    write_command(app.input, command);
    assert(read_line(app.output, line, sizeof(line)));
    unsigned int remaining = 0;
    assert(sscanf(line, "Driver %*u: Busy %u", &remaining) == 1);
    assert(remaining > 0 && remaining <= 2U);

    assert(kill((pid_t)driver_pids[1], SIGKILL) == 0);
    assert(read_line(app.output, line, sizeof(line)));
    long disconnected_pid = 0;
    assert(sscanf(line, "Driver disconnected: %ld", &disconnected_pid) == 1);
    assert(disconnected_pid == driver_pids[1]);

    write_command(app.input, "get_drivers\n");
    for (size_t response = 0; response < TEST_DRIVER_COUNT - 1U; ++response) {
        assert(read_line(app.output, line, sizeof(line)));
        long process_id = 0;
        assert(sscanf(line, "Driver %ld:", &process_id) == 1);
        assert(process_id != driver_pids[1]);
    }

    stop_app(&app);
}

static void test_immediate_eof(const char *program) {
    app_process_t app = start_app(program);
    write_command(app.input, "create_driver\n");
    close(app.input);
    app.input = -1;

    char line[256];
    assert(read_line(app.output, line, sizeof(line)));
    long driver_pid = 0;
    assert(sscanf(line, "Driver created: %ld", &driver_pid) == 1);

    int status = 0;
    const bool exited = wait_for_exit(app.process_id, &status);
    if (!exited) {
        kill(app.process_id, SIGKILL);
        kill((pid_t)driver_pid, SIGKILL);
        waitpid(app.process_id, &status, 0);
    }
    assert(exited);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == EXIT_SUCCESS);
    close(app.output);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_three_drivers_and_crash(argv[1]);
    test_immediate_eof(argv[1]);
    puts("app tests passed");
    return 0;
}
