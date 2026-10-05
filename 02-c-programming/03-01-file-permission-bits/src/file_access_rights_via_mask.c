#include "file_access_rights_via_mask.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>



ExecutionStatus parse_access_rights(const char *input, FileAccessRights *output_struct){
    if (input == NULL || output_struct == NULL) return EXECUTION_FAIL;
    static const unsigned int masks[9] = {
        0400, 0200, 0100,
        0040, 0020, 0010,
        0004, 0002, 0001
    };

    static const char permission_chars[9] = {
        'r', 'w', 'x',
        'r', 'w', 'x',
        'r', 'w', 'x'
    };

    unsigned int mode = 0;
    size_t input_length = strlen(input);
    
    if (input_length == 3){
        for (size_t i = 0; i < 3; i++){
            if (input[i] < '0' || input[i] > '7') return EXECUTION_BAD_INPUT;
            mode = (mode << 3) | (unsigned int)(input[i] - '0');
        }
    }
    else if ( input_length == 9){
        for (size_t i = 0; i < 9; i++){
            if (input[i] == permission_chars[i]) mode |= masks[i];
            else if (input[i] != '-') return EXECUTION_BAD_INPUT;
        }
    }
    else return EXECUTION_BAD_INPUT;

    for (size_t i = 0; i < 9; ++i) {
        if ((mode & masks[i]) != 0U) {
            output_struct->bit_view[i] = '1';
            output_struct->text_view[i] = permission_chars[i];
        } else {
            output_struct->bit_view[i] = '0';
            output_struct->text_view[i] = '-';
        }
    }

    output_struct->bit_view[9] = '\0';
    output_struct->text_view[9] = '\0';

    int written = snprintf(output_struct->num_view,NUM_VIEW_SIZE,"%03o",mode);

    if (written < 0 || written >= NUM_VIEW_SIZE) {
        return EXECUTION_FAIL;
    }

    return EXECUTION_OK;

    
}

ExecutionStatus access_rights_at_file(const char *file_name, FileAccessRights *output_struct){
    struct stat file_info;

    if (file_name == NULL || output_struct == NULL) return EXECUTION_BAD_INPUT;
    if (stat(file_name, &file_info) == -1) return EXECUTION_FAIL;

    unsigned int mode = (unsigned int)(file_info.st_mode & 0777);
    char numeric_view[NUM_VIEW_SIZE];

    int written = snprintf(numeric_view, sizeof(numeric_view),"%03o",mode);
    if(written < 0 || (size_t)written >= sizeof(numeric_view)) return EXECUTION_FAIL;

    ExecutionStatus status = parse_access_rights(numeric_view, output_struct);
    if (status != EXECUTION_OK) return status;

    written = snprintf(output_struct->file_name, FILE_NAME_SIZE, "%s", file_name);
    if (written < 0 || written >= FILE_NAME_SIZE) return EXECUTION_FAIL;

    return EXECUTION_OK;
}

ExecutionStatus change_access_rights(const char *command, FileAccessRights *access_struct){
    enum {
        WHO_USER = 1,
        WHO_GROUP = 2,
        WHO_OTHER = 4,
        WHO_ALL = WHO_USER | WHO_GROUP | WHO_OTHER
    };

    if (command == NULL || access_struct == NULL) return EXECUTION_BAD_INPUT;

    if (strlen(access_struct->num_view) != 3) return EXECUTION_BAD_INPUT;

    unsigned int mode = 0;
    for (size_t i = 0; i < 3; ++i) {
        if (access_struct->num_view[i] < '0' ||
            access_struct->num_view[i] > '7') {
            return EXECUTION_BAD_INPUT;
        }

        mode = (mode << 3) |
               (unsigned int)(access_struct->num_view[i] - '0');
    }

    size_t position = 0;

    while (command[position] != '\0') {
        unsigned int who = 0;

        while (command[position] == 'u' ||
               command[position] == 'g' ||
               command[position] == 'o' ||
               command[position] == 'a') {
            switch (command[position]) {
                case 'u':
                    who |= WHO_USER;
                    break;
                case 'g':
                    who |= WHO_GROUP;
                    break;
                case 'o':
                    who |= WHO_OTHER;
                    break;
                case 'a':
                    who |= WHO_ALL;
                    break;
            }
            ++position;
        }

        if (who == 0) return EXECUTION_BAD_INPUT;

        char operation = command[position];
        if (operation != '+' && operation != '-' && operation != '=') {
            return EXECUTION_BAD_INPUT;
        }
        ++position;

        unsigned int permission_mask = 0;
        size_t permission_count = 0;

        while (command[position] == 'r' ||
               command[position] == 'w' ||
               command[position] == 'x') {
            char permission = command[position];

            if (permission == 'r') {
                if ((who & WHO_USER) != 0U) permission_mask |= 0400;
                if ((who & WHO_GROUP) != 0U) permission_mask |= 0040;
                if ((who & WHO_OTHER) != 0U) permission_mask |= 0004;
            } else if (permission == 'w') {
                if ((who & WHO_USER) != 0U) permission_mask |= 0200;
                if ((who & WHO_GROUP) != 0U) permission_mask |= 0020;
                if ((who & WHO_OTHER) != 0U) permission_mask |= 0002;
            } else {
                if ((who & WHO_USER) != 0U) permission_mask |= 0100;
                if ((who & WHO_GROUP) != 0U) permission_mask |= 0010;
                if ((who & WHO_OTHER) != 0U) permission_mask |= 0001;
            }

            ++permission_count;
            ++position;
        }

        if (permission_count == 0 && operation != '=') {
            return EXECUTION_BAD_INPUT;
        }

        if (operation == '+') {
            mode |= permission_mask;
        } else if (operation == '-') {
            mode &= ~permission_mask;
        } else {
            unsigned int class_mask = 0;

            if ((who & WHO_USER) != 0U) class_mask |= 0700;
            if ((who & WHO_GROUP) != 0U) class_mask |= 0070;
            if ((who & WHO_OTHER) != 0U) class_mask |= 0007;

            mode = (mode & ~class_mask) | permission_mask;
        }

        if (command[position] == ',') {
            ++position;
            if (command[position] == '\0') return EXECUTION_BAD_INPUT;
        } else if (command[position] != '\0') {
            return EXECUTION_BAD_INPUT;
        }
    }

    char numeric_view[NUM_VIEW_SIZE];
    int written = snprintf(numeric_view,sizeof(numeric_view),"%03o",mode);

    if (written < 0 || (size_t)written >= sizeof(numeric_view)) {
        return EXECUTION_FAIL;
    }

    return parse_access_rights(numeric_view, access_struct);
}
