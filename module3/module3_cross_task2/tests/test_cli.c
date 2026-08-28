#include "cli.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static cli_command_t parse_complete(const char *line) {
    cli_command_t command;
    assert(cli_parse_command(line, &command) == CLI_PARSE_COMPLETE);
    return command;
}

static void test_commands_without_arguments(void) {
    cli_command_t command = parse_complete("create_driver");
    assert(command.type == CLI_COMMAND_CREATE_DRIVER);
    assert(command.driver_pid == 0);
    assert(command.task_timer == 0);

    command = parse_complete("  get_drivers\n");
    assert(command.type == CLI_COMMAND_GET_DRIVERS);
}

static void test_get_status(void) {
    const cli_command_t command = parse_complete("get_status\t12345  ");
    assert(command.type == CLI_COMMAND_GET_STATUS);
    assert(command.driver_pid == UINT32_C(12345));
    assert(command.task_timer == 0);
}

static void test_send_task(void) {
    const cli_command_t command = parse_complete(" send_task 42 60\n");
    assert(command.type == CLI_COMMAND_SEND_TASK);
    assert(command.driver_pid == UINT32_C(42));
    assert(command.task_timer == UINT32_C(60));
}

static void test_empty_input(void) {
    cli_command_t command;
    assert(cli_parse_command("", &command) == CLI_PARSE_EMPTY);
    assert(cli_parse_command(" \t\n", &command) == CLI_PARSE_EMPTY);
}

static void test_invalid_commands(void) {
    cli_command_t command;
    static const char *invalid_lines[] = {
        "unknown",
        "CREATE_DRIVER",
        "create_driver 1",
        "get_drivers extra",
        "get_status",
        "get_status 0",
        "get_status -1",
        "get_status 12x",
        "get_status 4294967296",
        "send_task",
        "send_task 1",
        "send_task 0 10",
        "send_task 1 0",
        "send_task -1 10",
        "send_task 1 -10",
        "send_task 1 10 extra"
    };

    for (size_t index = 0; index < sizeof(invalid_lines) / sizeof(invalid_lines[0]); ++index) {
        assert(cli_parse_command(invalid_lines[index], &command) == CLI_PARSE_INVALID);
    }

    assert(cli_parse_command(NULL, &command) == CLI_PARSE_INVALID);
    assert(cli_parse_command("get_drivers", NULL) == CLI_PARSE_INVALID);
}

static void test_line_limit(void) {
    char line[CLI_LINE_MAX + 2U];
    memset(line, 'a', sizeof(line));
    line[sizeof(line) - 1U] = '\0';

    cli_command_t command;
    assert(cli_parse_command(line, &command) == CLI_PARSE_INVALID);
}

static void test_usage(void) {
    const char *usage = cli_usage();
    assert(usage != NULL);
    assert(strstr(usage, "create_driver") != NULL);
    assert(strstr(usage, "send_task <pid> <task_timer>") != NULL);
    assert(strstr(usage, "get_status <pid>") != NULL);
    assert(strstr(usage, "get_drivers") != NULL);
}

int main(void) {
    test_commands_without_arguments();
    test_get_status();
    test_send_task();
    test_empty_input();
    test_invalid_commands();
    test_line_limit();
    test_usage();
    puts("cli tests passed");
    return 0;
}
