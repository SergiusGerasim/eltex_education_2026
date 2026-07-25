#pragma once 

#define FILE_NAME_SIZE 100
#define TEXT_VIEW_SIZE 10
#define NUM_VIEW_SIZE 4
#define BIT_VIEW_SIZE 10


/*
 Написать программу для расчета маски прав доступа к файлу.
1)Пользователь может ввести права доступа в буквенном или цифровом
обозначении, ему должно быть показано соответствующее битовое
представление.
2)Пользователь может ввести имя файла, и ему отобразится буквенное,
цифровое и битовое представление прав доступа. Использовать функцию stat
для получения информации о файле. Сравнить результат с выводом,
например, ls -l.
3)Пользователь может изменить права доступа, определенные в первом или
втором пункте, введя команды модификации атрибутов (подобно команде
chmod). При этом отображается буквенное, цифровое и битовое
представление прав доступа. Изменение прав доступа не нужно применять к
файлу.
*/

typedef enum{
    EXECUTION_OK,
    EXECUTION_FAIL,
    EXECUTION_BAD_INPUT
} ExecutionStatus;

typedef struct {
    char bit_view[BIT_VIEW_SIZE];
    char num_view[NUM_VIEW_SIZE];
    char text_view[TEXT_VIEW_SIZE];
    char file_name[FILE_NAME_SIZE];
} FileAccessRights;


ExecutionStatus parse_access_rights(const char *input, FileAccessRights *output_struct);

ExecutionStatus access_rights_at_file(const char *file_name, FileAccessRights *output_struct);

ExecutionStatus change_access_rights(const char *command, FileAccessRights *access_struct);
