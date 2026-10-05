#include "unity.h"
#include "phone_book.h"

#include <string.h>

static phone_book_node_t *head;
static phone_book_id_manager_t id_manager;

void setUp(void){
    head = NULL;
    phone_book_id_manager_init(&id_manager);
}

void tearDown(void){
    phone_book_clear(&head, &id_manager);
}

static int add_contact(const char *name, const char *last_name){
    return phone_book_node_create_and_add(
        name,
        last_name,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &head,
        &id_manager
    );
}

static void make_string(char *string, size_t length, char symbol){
    memset(string, symbol, length);
    string[length] = '\0';
}

void test_id_manager_init_should_set_initial_state(void){
    id_manager.next_id = 100;
    id_manager.free_ids = (phone_book_free_id_node_t *)1;

    phone_book_id_manager_init(&id_manager);

    TEST_ASSERT_EQUAL_UINT(1, id_manager.next_id);
    TEST_ASSERT_NULL(id_manager.free_ids);

    phone_book_id_manager_init(NULL);
}

void test_create_and_add_should_fill_all_contact_fields(void){
    int result = phone_book_node_create_and_add(
        "Сергей",
        "Герасимов",
        "Михайлович",
        "89915047818",
        "Новосибирск",
        "Элтекс",
        "Инженер",
        &head,
        &id_manager
    );

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_NOT_NULL(head);
    TEST_ASSERT_EQUAL_UINT(1, head->id);
    TEST_ASSERT_EQUAL_STRING("Сергей", head->name);
    TEST_ASSERT_EQUAL_STRING("Герасимов", head->last_name);
    TEST_ASSERT_EQUAL_STRING("Михайлович", head->patronymic);
    TEST_ASSERT_EQUAL_STRING("Новосибирск", head->home_address);
    TEST_ASSERT_EQUAL_STRING("Элтекс", head->workplace);
    TEST_ASSERT_EQUAL_STRING("Инженер", head->job_title);
    TEST_ASSERT_EQUAL_size_t(1, head->phone_count);
    TEST_ASSERT_EQUAL_STRING("89915047818", head->phone_numbers[0]);
    TEST_ASSERT_EQUAL_size_t(0, head->email_count);
    TEST_ASSERT_EQUAL_size_t(0, head->social_link_count);
    TEST_ASSERT_NULL(head->next);
}

void test_create_and_add_should_initialize_optional_fields_as_empty(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    TEST_ASSERT_EQUAL_STRING("", head->patronymic);
    TEST_ASSERT_EQUAL_STRING("", head->home_address);
    TEST_ASSERT_EQUAL_STRING("", head->workplace);
    TEST_ASSERT_EQUAL_STRING("", head->job_title);
    TEST_ASSERT_EQUAL_size_t(0, head->phone_count);
}

void test_create_and_add_should_reject_invalid_arguments(void){
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_create_and_add(
        NULL, "Герасимов", NULL, NULL, NULL, NULL, NULL, &head, &id_manager
    ));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_create_and_add(
        "Сергей", NULL, NULL, NULL, NULL, NULL, NULL, &head, &id_manager
    ));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_create_and_add(
        "Сергей", "Герасимов", NULL, NULL, NULL, NULL, NULL, NULL, &id_manager
    ));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_create_and_add(
        "Сергей", "Герасимов", NULL, NULL, NULL, NULL, NULL, &head, NULL
    ));

    TEST_ASSERT_NULL(head);
    TEST_ASSERT_EQUAL_UINT(1, id_manager.next_id);
}

void test_create_and_add_should_append_nodes_and_assign_new_ids(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Первый", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Второй", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Третий", "Контакт"));

    TEST_ASSERT_EQUAL_UINT(1, head->id);
    TEST_ASSERT_EQUAL_UINT(2, head->next->id);
    TEST_ASSERT_EQUAL_UINT(3, head->next->next->id);
    TEST_ASSERT_NULL(head->next->next->next);
    TEST_ASSERT_EQUAL_UINT(4, id_manager.next_id);
}

void test_add_phone_should_add_values_and_check_errors(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    for (size_t i = 0; i < MAX_PHONE_NUMBERS; i++){
        TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_phone(head, "89991234567"));
    }

    TEST_ASSERT_EQUAL_size_t(MAX_PHONE_NUMBERS, head->phone_count);
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_phone(head, "81111111111"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_phone(NULL, "81111111111"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_phone(head, NULL));
}

void test_add_phone_should_reject_too_long_value_without_changing_count(void){
    char too_long[PHONESIZE + 1];
    make_string(too_long, PHONESIZE, '1');
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_phone(head, too_long));
    TEST_ASSERT_EQUAL_size_t(0, head->phone_count);
}

void test_add_email_should_add_values_and_check_errors(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    for (size_t i = 0; i < MAX_EMAILS; i++){
        TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_email(head, "ivan@example.com"));
    }

    TEST_ASSERT_EQUAL_size_t(MAX_EMAILS, head->email_count);
    TEST_ASSERT_EQUAL_STRING("ivan@example.com", head->emails[0]);
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_email(head, "extra@example.com"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_email(NULL, "ivan@example.com"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_email(head, NULL));
}

void test_add_social_link_should_add_values_and_check_errors(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    for (size_t i = 0; i < MAX_SOCIAL_LINKS; i++){
        TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_social_link(head, "https://t.me/ivan"));
    }

    TEST_ASSERT_EQUAL_size_t(MAX_SOCIAL_LINKS, head->social_link_count);
    TEST_ASSERT_EQUAL_STRING("https://t.me/ivan", head->social_links[0]);
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_social_link(head, "https://vk.com/ivan"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_social_link(NULL, "https://t.me/ivan"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_node_add_social_link(head, NULL));
}

void test_find_by_id_should_return_contact_or_null(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Первый", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Второй", "Контакт"));

    phone_book_node_t *found = phone_book_find_by_id(head, 2);

    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("Второй", found->name);
    TEST_ASSERT_NULL(phone_book_find_by_id(head, 100));
    TEST_ASSERT_NULL(phone_book_find_by_id(NULL, 1));
}

void test_edit_text_fields_should_change_all_values(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_name(head, 1, "Петр"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_last_name(head, 1, "Петров"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_patronymic(head, 1, "Петрович"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_home_address(head, 1, "Москва"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_workplace(head, 1, "Завод"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_job_title(head, 1, "Механик"));

    TEST_ASSERT_EQUAL_STRING("Петр", head->name);
    TEST_ASSERT_EQUAL_STRING("Петров", head->last_name);
    TEST_ASSERT_EQUAL_STRING("Петрович", head->patronymic);
    TEST_ASSERT_EQUAL_STRING("Москва", head->home_address);
    TEST_ASSERT_EQUAL_STRING("Завод", head->workplace);
    TEST_ASSERT_EQUAL_STRING("Механик", head->job_title);
}

void test_edit_text_fields_should_reject_invalid_data_and_keep_old_value(void){
    char too_long[NAMESIZE + 1];
    make_string(too_long, NAMESIZE, 'A');
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_name(head, 100, "Петр"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_name(head, 1, NULL));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_name(head, 1, too_long));
    TEST_ASSERT_EQUAL_STRING("Сергей", head->name);
}

void test_edit_collection_items_should_change_existing_values(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_phone(head, "89991234567"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_email(head, "old@example.com"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_social_link(head, "https://t.me/old"));

    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_phone(head, 1, 0, "81111111111"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_email(head, 1, 0, "new@example.com"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_social_link(head, 1, 0, "https://t.me/new"));

    TEST_ASSERT_EQUAL_STRING("81111111111", head->phone_numbers[0]);
    TEST_ASSERT_EQUAL_STRING("new@example.com", head->emails[0]);
    TEST_ASSERT_EQUAL_STRING("https://t.me/new", head->social_links[0]);
}

void test_edit_collection_items_should_reject_invalid_id_index_and_value(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_phone(head, "89991234567"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_email(head, "old@example.com"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_node_add_social_link(head, "https://t.me/old"));

    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_phone(head, 100, 0, "81111111111"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_phone(head, 1, 1, "81111111111"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_phone(head, 1, 0, NULL));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_email(head, 1, 1, "new@example.com"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_email(head, 1, 0, NULL));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_social_link(head, 1, 1, "https://t.me/new"));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_social_link(head, 1, 0, NULL));

    TEST_ASSERT_EQUAL_STRING("89991234567", head->phone_numbers[0]);
    TEST_ASSERT_EQUAL_STRING("old@example.com", head->emails[0]);
    TEST_ASSERT_EQUAL_STRING("https://t.me/old", head->social_links[0]);
}

void test_search_should_find_by_name_last_name_and_both(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Василий", "Герасимов"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Дорожный"));

    phone_book_search_ans_t by_name = phone_book_find_by_name(head, "Сергей");
    TEST_ASSERT_EQUAL_size_t(2, by_name.count);
    TEST_ASSERT_EQUAL_UINT(1, by_name.items[0]->id);
    TEST_ASSERT_EQUAL_UINT(3, by_name.items[1]->id);

    phone_book_search_ans_t by_last_name = phone_book_find_by_last_name(head, "Герасимов");
    TEST_ASSERT_EQUAL_size_t(2, by_last_name.count);
    TEST_ASSERT_EQUAL_UINT(1, by_last_name.items[0]->id);
    TEST_ASSERT_EQUAL_UINT(2, by_last_name.items[1]->id);

    phone_book_search_ans_t by_both = phone_book_find_by_name_and_last_name(
        head, "Сергей", "Герасимов"
    );
    TEST_ASSERT_EQUAL_size_t(1, by_both.count);
    TEST_ASSERT_EQUAL_UINT(1, by_both.items[0]->id);

    phone_book_search_result_free(&by_name);
    phone_book_search_result_free(&by_last_name);
    phone_book_search_result_free(&by_both);
}

void test_search_should_return_empty_result_for_missing_or_null_criteria(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    phone_book_search_ans_t missing = phone_book_find_by_name(head, "Петр");
    phone_book_search_ans_t null_name = phone_book_find_by_name(head, NULL);
    phone_book_search_ans_t null_last_name = phone_book_find_by_last_name(head, NULL);
    phone_book_search_ans_t null_both = phone_book_find_by_name_and_last_name(head, NULL, NULL);

    TEST_ASSERT_EQUAL_size_t(0, missing.count);
    TEST_ASSERT_NULL(missing.items);
    TEST_ASSERT_EQUAL_size_t(0, null_name.count);
    TEST_ASSERT_NULL(null_name.items);
    TEST_ASSERT_EQUAL_size_t(0, null_last_name.count);
    TEST_ASSERT_NULL(null_last_name.items);
    TEST_ASSERT_EQUAL_size_t(0, null_both.count);
    TEST_ASSERT_NULL(null_both.items);

    phone_book_search_result_free(&missing);
    phone_book_search_result_free(&null_name);
    phone_book_search_result_free(&null_last_name);
    phone_book_search_result_free(&null_both);
    phone_book_search_result_free(NULL);
}

void test_search_result_free_should_reset_result(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));
    phone_book_search_ans_t result = phone_book_find_by_name(head, "Сергей");
    TEST_ASSERT_NOT_NULL(result.items);

    phone_book_search_result_free(&result);

    TEST_ASSERT_NULL(result.items);
    TEST_ASSERT_EQUAL_size_t(0, result.count);
    phone_book_search_result_free(&result);
}

void test_remove_should_remove_first_middle_and_last_nodes(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Первый", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Второй", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Третий", "Контакт"));

    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 2, &id_manager));
    TEST_ASSERT_EQUAL_UINT(1, head->id);
    TEST_ASSERT_EQUAL_UINT(3, head->next->id);
    TEST_ASSERT_NULL(head->next->next);

    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 1, &id_manager));
    TEST_ASSERT_EQUAL_UINT(3, head->id);

    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 3, &id_manager));
    TEST_ASSERT_NULL(head);
}

void test_remove_should_reject_invalid_arguments_and_missing_id(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Сергей", "Герасимов"));

    TEST_ASSERT_EQUAL_INT(-1, phone_book_remove(&head, 100, &id_manager));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_remove(NULL, 1, &id_manager));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_remove(&head, 1, NULL));
    TEST_ASSERT_NOT_NULL(head);
}

void test_removed_id_should_be_reused_before_never_used_id(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Первый", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Второй", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 1, &id_manager));

    TEST_ASSERT_EQUAL_INT(0, add_contact("Третий", "Контакт"));
    TEST_ASSERT_EQUAL_UINT(1, head->next->id);

    TEST_ASSERT_EQUAL_INT(0, add_contact("Четвертый", "Контакт"));
    TEST_ASSERT_EQUAL_UINT(3, head->next->next->id);
}

void test_clear_should_free_lists_and_reset_id_manager(void){
    TEST_ASSERT_EQUAL_INT(0, add_contact("Первый", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, add_contact("Второй", "Контакт"));
    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 1, &id_manager));
    TEST_ASSERT_NOT_NULL(id_manager.free_ids);

    phone_book_clear(&head, &id_manager);

    TEST_ASSERT_NULL(head);
    TEST_ASSERT_NULL(id_manager.free_ids);
    TEST_ASSERT_EQUAL_UINT(1, id_manager.next_id);

    TEST_ASSERT_EQUAL_INT(0, add_contact("Новый", "Контакт"));
    TEST_ASSERT_EQUAL_UINT(1, head->id);

    phone_book_clear(NULL, &id_manager);
    phone_book_clear(&head, NULL);
    TEST_ASSERT_NULL(head);
}

int main(void){
    UNITY_BEGIN();

    RUN_TEST(test_id_manager_init_should_set_initial_state);
    RUN_TEST(test_create_and_add_should_fill_all_contact_fields);
    RUN_TEST(test_create_and_add_should_initialize_optional_fields_as_empty);
    RUN_TEST(test_create_and_add_should_reject_invalid_arguments);
    RUN_TEST(test_create_and_add_should_append_nodes_and_assign_new_ids);
    RUN_TEST(test_add_phone_should_add_values_and_check_errors);
    RUN_TEST(test_add_phone_should_reject_too_long_value_without_changing_count);
    RUN_TEST(test_add_email_should_add_values_and_check_errors);
    RUN_TEST(test_add_social_link_should_add_values_and_check_errors);
    RUN_TEST(test_find_by_id_should_return_contact_or_null);
    RUN_TEST(test_edit_text_fields_should_change_all_values);
    RUN_TEST(test_edit_text_fields_should_reject_invalid_data_and_keep_old_value);
    RUN_TEST(test_edit_collection_items_should_change_existing_values);
    RUN_TEST(test_edit_collection_items_should_reject_invalid_id_index_and_value);
    RUN_TEST(test_search_should_find_by_name_last_name_and_both);
    RUN_TEST(test_search_should_return_empty_result_for_missing_or_null_criteria);
    RUN_TEST(test_search_result_free_should_reset_result);
    RUN_TEST(test_remove_should_remove_first_middle_and_last_nodes);
    RUN_TEST(test_remove_should_reject_invalid_arguments_and_missing_id);
    RUN_TEST(test_removed_id_should_be_reused_before_never_used_id);
    RUN_TEST(test_clear_should_free_lists_and_reset_id_manager);

    return UNITY_END();
}
