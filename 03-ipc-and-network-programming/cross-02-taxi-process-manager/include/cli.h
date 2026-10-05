#pragma once

#include <stdint.h>

#define CLI_LINE_MAX 255U

typedef enum {
    CLI_COMMAND_CREATE_DRIVER = 1,
    CLI_COMMAND_SEND_TASK = 2,
    CLI_COMMAND_GET_STATUS = 3,
    CLI_COMMAND_GET_DRIVERS = 4
} cli_command_type_t;

typedef enum {
    CLI_PARSE_INVALID = -1,
    CLI_PARSE_EMPTY = 0,
    CLI_PARSE_COMPLETE = 1
} cli_parse_result_t;

typedef struct {
    cli_command_type_t type;
    uint32_t driver_pid;
    uint32_t task_timer;
} cli_command_t;

cli_parse_result_t cli_parse_command(const char *line, cli_command_t *command);
const char *cli_usage(void);
