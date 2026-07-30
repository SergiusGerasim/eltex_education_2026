#include "phone_book_ui.h"
#include "phone_book.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_line(const char *prompt, char *buffer, size_t buffer_size){
    if (prompt == NULL || buffer == NULL || buffer_size == 0 || buffer_size > INT_MAX) return -1;

    printf("%s", prompt);

    if (fgets(buffer, (int)buffer_size, stdin) == NULL) return -1;

    size_t length = strlen(buffer);

    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
        return 0;
    }

    int character;
    while ((character = getchar()) != '\n' && character != EOF) {
    }

    printf("Введенная строка слишком длинная.\n");
    return -1;
}

static int read_required_line(const char *prompt, char *buffer, size_t buffer_size){
    while (read_line(prompt, buffer, buffer_size) == 0) {
        if (buffer[0] != '\0') return 0;
        printf("Поле обязательно для заполнения.\n");
    }

    return -1;
}

static int read_unsigned(const char *prompt, unsigned int *value){
    char buffer[32];

    if (value == NULL || read_line(prompt, buffer, sizeof(buffer)) != 0) return -1;
    if (buffer[0] == '\0' || buffer[0] == '-') return -1;

    errno = 0;
    char *end;
    unsigned long parsed = strtoul(buffer, &end, 10);

    if (errno == ERANGE || parsed > UINT_MAX || *end != '\0') return -1;

    *value = (unsigned int)parsed;
    return 0;
}

static void print_contact(const phone_book_node_t *contact){
    if (contact == NULL) return;

    printf("\nID: %u\n", contact->id);
    printf("Фамилия: %s\n", contact->last_name);
    printf("Имя: %s\n", contact->name);
    printf("Отчество: %s\n", contact->patronymic[0] != '\0' ? contact->patronymic : "не указано");
    printf("Домашний адрес: %s\n", contact->home_address[0] != '\0' ? contact->home_address : "не указан");
    printf("Место работы: %s\n", contact->workplace[0] != '\0' ? contact->workplace : "не указано");
    printf("Должность: %s\n", contact->job_title[0] != '\0' ? contact->job_title : "не указана");

    printf("Телефоны:\n");
    if (contact->phone_count == 0) printf("  не указаны\n");
    for (size_t i = 0; i < contact->phone_count; i++){
        printf("  %zu. %s\n", i + 1, contact->phone_numbers[i]);
    }

    printf("Электронная почта:\n");
    if (contact->email_count == 0) printf("  не указана\n");
    for (size_t i = 0; i < contact->email_count; i++){
        printf("  %zu. %s\n", i + 1, contact->emails[i]);
    }

    printf("Социальные сети и мессенджеры:\n");
    if (contact->social_link_count == 0) printf("  не указаны\n");
    for (size_t i = 0; i < contact->social_link_count; i++){
        printf("  %zu. %s\n", i + 1, contact->social_links[i]);
    }
}

static void print_all_contacts(const phone_book_node_t *head){
    if (head == NULL) {
        printf("Телефонная книга пуста.\n");
        return;
    }

    while (head != NULL) {
        print_contact(head);
        head = head->next;
    }
}

static phone_book_node_t *get_last_contact(phone_book_node_t *head){
    if (head == NULL) return NULL;

    while (head->next != NULL) head = head->next;
    return head;
}

static void add_contact(phone_book_node_t **head, phone_book_id_manager_t *id_manager){
    char name[NAMESIZE + 1];
    char last_name[NAMESIZE + 1];
    char patronymic[NAMESIZE + 1];
    char phone_number[PHONESIZE + 1];
    char home_address[ADDRESSSIZE + 1];
    char workplace[WORKPLACE_SIZE + 1];
    char job_title[JOB_TITLE_SIZE + 1];

    printf("\nДобавление контакта\n");

    if (read_required_line("Имя: ", name, sizeof(name)) != 0) return;
    if (read_required_line("Фамилия: ", last_name, sizeof(last_name)) != 0) return;
    if (read_line("Отчество (необязательно): ", patronymic, sizeof(patronymic)) != 0) return;
    if (read_line("Первый телефон (необязательно): ", phone_number, sizeof(phone_number)) != 0) return;
    if (read_line("Домашний адрес (необязательно): ", home_address, sizeof(home_address)) != 0) return;
    if (read_line("Место работы (необязательно): ", workplace, sizeof(workplace)) != 0) return;
    if (read_line("Должность (необязательно): ", job_title, sizeof(job_title)) != 0) return;

    int result = phone_book_node_create_and_add(
        name,
        last_name,
        patronymic[0] != '\0' ? patronymic : NULL,
        phone_number[0] != '\0' ? phone_number : NULL,
        home_address[0] != '\0' ? home_address : NULL,
        workplace[0] != '\0' ? workplace : NULL,
        job_title[0] != '\0' ? job_title : NULL,
        head,
        id_manager
    );

    if (result != 0) {
        printf("Не удалось добавить контакт.\n");
        return;
    }

    phone_book_node_t *new_contact = get_last_contact(*head);
    printf("Контакт добавлен. ID: %u\n", new_contact->id);
}

static void edit_text_field(phone_book_node_t *head, unsigned int id, unsigned int command){
    char value[SOCIAL_LINK_SIZE + 1];

    if (read_line("Новое значение (пустая строка очищает поле): ", value, sizeof(value)) != 0) return;

    int result = -1;

    switch (command) {
        case 1:
            if (value[0] == '\0') {
                printf("Имя нельзя оставить пустым.\n");
                return;
            }
            result = phone_book_edit_name(head, id, value);
            break;
        case 2:
            if (value[0] == '\0') {
                printf("Фамилию нельзя оставить пустой.\n");
                return;
            }
            result = phone_book_edit_last_name(head, id, value);
            break;
        case 3:
            result = phone_book_edit_patronymic(head, id, value);
            break;
        case 4:
            result = phone_book_edit_home_address(head, id, value);
            break;
        case 5:
            result = phone_book_edit_workplace(head, id, value);
            break;
        case 6:
            result = phone_book_edit_job_title(head, id, value);
            break;
        default:
            return;
    }

    if (result == 0) printf("Поле изменено.\n");
    else printf("Не удалось изменить поле: значение слишком длинное.\n");
}

static void add_contact_item(phone_book_node_t *contact, unsigned int command){
    char value[SOCIAL_LINK_SIZE + 1];

    if (read_required_line("Новое значение: ", value, sizeof(value)) != 0) return;

    int result = -1;

    if (command == 7) result = phone_book_node_add_phone(contact, value);
    else if (command == 9) result = phone_book_node_add_email(contact, value);
    else if (command == 11) result = phone_book_node_add_social_link(contact, value);

    if (result == 0) printf("Значение добавлено.\n");
    else printf("Не удалось добавить значение: достигнут лимит или строка слишком длинная.\n");
}

static void edit_contact_item(phone_book_node_t *head, phone_book_node_t *contact,
    unsigned int id, unsigned int command){
    unsigned int number;

    if (read_unsigned("Номер изменяемого значения: ", &number) != 0 || number == 0) {
        printf("Некорректный номер.\n");
        return;
    }

    char value[SOCIAL_LINK_SIZE + 1];
    if (read_required_line("Новое значение: ", value, sizeof(value)) != 0) return;

    size_t index = (size_t)(number - 1);
    int result = -1;

    if (command == 8 && index < contact->phone_count) {
        result = phone_book_edit_phone(head, id, index, value);
    }
    else if (command == 10 && index < contact->email_count) {
        result = phone_book_edit_email(head, id, index, value);
    }
    else if (command == 12 && index < contact->social_link_count) {
        result = phone_book_edit_social_link(head, id, index, value);
    }

    if (result == 0) printf("Значение изменено.\n");
    else printf("Не удалось изменить значение: неверный номер или строка слишком длинная.\n");
}

static void edit_contact(phone_book_node_t *head){
    unsigned int id;

    if (read_unsigned("ID контакта: ", &id) != 0) {
        printf("Некорректный ID.\n");
        return;
    }

    phone_book_node_t *contact = phone_book_find_by_id(head, id);
    if (contact == NULL) {
        printf("Контакт не найден.\n");
        return;
    }

    unsigned int command = UINT_MAX;

    while (command != 0) {
        print_contact(contact);
        printf("\nРедактирование контакта\n");
        printf("1. Имя\n");
        printf("2. Фамилия\n");
        printf("3. Отчество\n");
        printf("4. Домашний адрес\n");
        printf("5. Место работы\n");
        printf("6. Должность\n");
        printf("7. Добавить телефон\n");
        printf("8. Изменить телефон\n");
        printf("9. Добавить email\n");
        printf("10. Изменить email\n");
        printf("11. Добавить ссылку\n");
        printf("12. Изменить ссылку\n");
        printf("0. Назад\n");

        if (read_unsigned("Выберите действие: ", &command) != 0) {
            printf("Некорректная команда.\n");
            continue;
        }

        if (command >= 1 && command <= 6) edit_text_field(head, id, command);
        else if (command == 7 || command == 9 || command == 11) add_contact_item(contact, command);
        else if (command == 8 || command == 10 || command == 12) {
            edit_contact_item(head, contact, id, command);
        }
        else if (command != 0) printf("Неизвестная команда.\n");
    }
}

static void remove_contact(phone_book_node_t **head, phone_book_id_manager_t *id_manager){
    unsigned int id;

    if (read_unsigned("ID удаляемого контакта: ", &id) != 0) {
        printf("Некорректный ID.\n");
        return;
    }

    if (phone_book_remove(head, id, id_manager) == 0) printf("Контакт удален.\n");
    else printf("Контакт не найден или не удалось освободить ID.\n");
}

static void print_search_result(phone_book_search_ans_t *result){
    if (result->count == 0) {
        printf("Контакты не найдены.\n");
        return;
    }

    for (size_t i = 0; i < result->count; i++) print_contact(result->items[i]);
}

static void search_contacts(phone_book_node_t *head){
    unsigned int command;

    printf("\nПоиск контактов\n");
    printf("1. По ID\n");
    printf("2. По имени\n");
    printf("3. По фамилии\n");
    printf("4. По имени и фамилии\n");
    printf("0. Назад\n");

    if (read_unsigned("Выберите способ поиска: ", &command) != 0) {
        printf("Некорректная команда.\n");
        return;
    }

    if (command == 0) return;

    if (command == 1) {
        unsigned int id;
        if (read_unsigned("ID: ", &id) != 0) {
            printf("Некорректный ID.\n");
            return;
        }

        phone_book_node_t *contact = phone_book_find_by_id(head, id);
        if (contact == NULL) printf("Контакт не найден.\n");
        else print_contact(contact);
        return;
    }

    char name[NAMESIZE + 1];
    char last_name[NAMESIZE + 1];
    phone_book_search_ans_t result = {.items = NULL, .count = 0};

    if (command == 2) {
        if (read_required_line("Имя: ", name, sizeof(name)) != 0) return;
        result = phone_book_find_by_name(head, name);
    }
    else if (command == 3) {
        if (read_required_line("Фамилия: ", last_name, sizeof(last_name)) != 0) return;
        result = phone_book_find_by_last_name(head, last_name);
    }
    else if (command == 4) {
        if (read_required_line("Имя: ", name, sizeof(name)) != 0) return;
        if (read_required_line("Фамилия: ", last_name, sizeof(last_name)) != 0) return;
        result = phone_book_find_by_name_and_last_name(head, name, last_name);
    }
    else {
        printf("Неизвестная команда.\n");
        return;
    }

    print_search_result(&result);
    phone_book_search_result_free(&result);
}

void phone_book_run(void){
    phone_book_node_t *head = NULL;
    phone_book_id_manager_t id_manager;
    phone_book_id_manager_init(&id_manager);

    unsigned int command = UINT_MAX;

    while (command != 0) {
        printf("\nТелефонная книга\n");
        printf("1. Добавить контакт\n");
        printf("2. Показать все контакты\n");
        printf("3. Редактировать контакт\n");
        printf("4. Удалить контакт\n");
        printf("5. Найти контакт\n");
        printf("0. Выход\n");

        if (read_unsigned("Выберите действие: ", &command) != 0) {
            if (feof(stdin)) break;
            printf("Некорректная команда.\n");
            continue;
        }

        switch (command) {
            case 1:
                add_contact(&head, &id_manager);
                break;
            case 2:
                print_all_contacts(head);
                break;
            case 3:
                edit_contact(head);
                break;
            case 4:
                remove_contact(&head, &id_manager);
                break;
            case 5:
                search_contacts(head);
                break;
            case 0:
                break;
            default:
                printf("Неизвестная команда.\n");
                break;
        }
    }

    phone_book_clear(&head, &id_manager);
    printf("Работа завершена.\n");
}
