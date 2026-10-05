#include "cli.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CLI_TOKEN_COUNT_MAX 4U

static size_t split_tokens(char *line, char **tokens, size_t capacity) {
    size_t count = 0;
    char *cursor = line;

    while (*cursor != '\0') {
        while (isspace((unsigned char)*cursor) != 0) ++cursor;
        if (*cursor == '\0') break;
        if (count == capacity) return capacity;

        tokens[count++] = cursor;
        while (*cursor != '\0' && isspace((unsigned char)*cursor) == 0) ++cursor;
        if (*cursor != '\0') *cursor++ = '\0';
    }
    return count;
}

static bool parse_positive_uint32(const char *text, uint32_t *value) {
    if (text == NULL || value == NULL || *text == '\0' || *text == '-') return false;

    errno = 0;
    char *end = NULL;
    const unsigned long parsed_value = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed_value == 0 || parsed_value > UINT32_MAX) return false;

    *value = (uint32_t)parsed_value;
    return true;
}

static bool parse_without_arguments(
    char **tokens,
    size_t token_count,
    const char *name,
    cli_command_type_t type,
    cli_command_t *command
) {
    if (token_count != 1 || strcmp(tokens[0], name) != 0) return false;
    command->type = type;
    return true;
}

cli_parse_result_t cli_parse_command(const char *line, cli_command_t *command) {
    if (line == NULL || command == NULL) return CLI_PARSE_INVALID;

    const size_t line_size = strnlen(line, CLI_LINE_MAX + 1U);
    if (line_size > CLI_LINE_MAX) return CLI_PARSE_INVALID;

    char copy[CLI_LINE_MAX + 1U];
    memcpy(copy, line, line_size + 1U);

    char *tokens[CLI_TOKEN_COUNT_MAX];
    const size_t token_count = split_tokens(copy, tokens, CLI_TOKEN_COUNT_MAX);
    if (token_count == 0) return CLI_PARSE_EMPTY;

    *command = (cli_command_t){0};
    if (parse_without_arguments(
            tokens,
            token_count,
            "create_driver",
            CLI_COMMAND_CREATE_DRIVER,
            command
        ) ||
        parse_without_arguments(
            tokens,
            token_count,
            "get_drivers",
            CLI_COMMAND_GET_DRIVERS,
            command
        )) {
        return CLI_PARSE_COMPLETE;
    }

    if (strcmp(tokens[0], "get_status") == 0) {
        if (token_count != 2 || !parse_positive_uint32(tokens[1], &command->driver_pid)) {
            return CLI_PARSE_INVALID;
        }
        command->type = CLI_COMMAND_GET_STATUS;
        return CLI_PARSE_COMPLETE;
    }

    if (strcmp(tokens[0], "send_task") == 0) {
        if (token_count != 3 ||
            !parse_positive_uint32(tokens[1], &command->driver_pid) ||
            !parse_positive_uint32(tokens[2], &command->task_timer)) {
            return CLI_PARSE_INVALID;
        }
        command->type = CLI_COMMAND_SEND_TASK;
        return CLI_PARSE_COMPLETE;
    }

    return CLI_PARSE_INVALID;
}

const char *cli_usage(void) {
    return "Commands:\n"
           "  create_driver\n"
           "  send_task <pid> <task_timer>\n"
           "  get_status <pid>\n"
           "  get_drivers\n";
}
