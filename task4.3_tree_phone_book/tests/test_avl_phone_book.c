#include "unity.h"
#include "phone_book.h"

#include <stdio.h>
#include <stdlib.h>
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

static void add_contact(const char *last_name){
    TEST_ASSERT_EQUAL_INT(0, phone_book_node_create_and_add(
        "Имя", last_name, NULL, NULL, NULL, NULL, NULL, &head, &id_manager
    ));
}

static int validate_node(const phone_book_node_t *node, const char **previous,
    size_t *count){
    if (node == NULL) return 0;

    int left_height = validate_node(node->left, previous, count);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, left_height);

    if (*previous != NULL) {
        TEST_ASSERT_LESS_OR_EQUAL_INT(0, strcmp(*previous, node->last_name));
    }
    *previous = node->last_name;
    (*count)++;

    int right_height = validate_node(node->right, previous, count);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, right_height);

    int difference = right_height - left_height;
    TEST_ASSERT_TRUE(difference >= -1 && difference <= 1);

    int actual_height = (left_height > right_height ? left_height : right_height) + 1;
    TEST_ASSERT_EQUAL_INT(actual_height, node->height);
    return actual_height;
}

static void assert_valid_avl(size_t expected_count){
    const char *previous = NULL;
    size_t count = 0;
    validate_node(head, &previous, &count);
    TEST_ASSERT_EQUAL_size_t(expected_count, count);
}

static void assert_three_node_tree(void){
    TEST_ASSERT_NOT_NULL(head);
    TEST_ASSERT_EQUAL_STRING("B", head->last_name);
    TEST_ASSERT_EQUAL_STRING("A", head->left->last_name);
    TEST_ASSERT_EQUAL_STRING("C", head->right->last_name);
    assert_valid_avl(3);
}

void test_insert_should_perform_ll_rotation(void){
    add_contact("C");
    add_contact("B");
    add_contact("A");
    assert_three_node_tree();
}

void test_insert_should_perform_rr_rotation(void){
    add_contact("A");
    add_contact("B");
    add_contact("C");
    assert_three_node_tree();
}

void test_insert_should_perform_lr_rotation(void){
    add_contact("C");
    add_contact("A");
    add_contact("B");
    assert_three_node_tree();
}

void test_insert_should_perform_rl_rotation(void){
    add_contact("A");
    add_contact("C");
    add_contact("B");
    assert_three_node_tree();
}

void test_sorted_insertions_should_keep_tree_balanced(void){
    char last_name[16];

    for (unsigned int i = 0; i < 200; i++) {
        snprintf(last_name, sizeof(last_name), "%03u", i);
        add_contact(last_name);
        assert_valid_avl((size_t)i + 1);
    }

    TEST_ASSERT_LESS_OR_EQUAL_INT(9, head->height);
    for (unsigned int id = 1; id <= 200; id++) {
        TEST_ASSERT_NOT_NULL(phone_book_find_by_id(head, id));
    }
}

void test_reverse_insertions_should_keep_tree_balanced(void){
    char last_name[16];

    for (unsigned int i = 200; i > 0; i--) {
        snprintf(last_name, sizeof(last_name), "%03u", i);
        add_contact(last_name);
    }

    assert_valid_avl(200);
    TEST_ASSERT_LESS_OR_EQUAL_INT(9, head->height);
}

void test_duplicate_last_names_should_not_lose_nodes(void){
    for (size_t i = 0; i < 50; i++) add_contact("Одинаковая");

    assert_valid_avl(50);

    phone_book_search_ans_t result =
        phone_book_find_by_last_name(head, "Одинаковая");
    TEST_ASSERT_EQUAL_size_t(50, result.count);
    for (unsigned int id = 1; id <= 50; id++) {
        TEST_ASSERT_NOT_NULL(phone_book_find_by_id(head, id));
    }
    phone_book_search_result_free(&result);
}

void test_remove_leaf_one_child_two_children_and_root(void){
    const char *names[] = {"D", "B", "F", "A", "C", "E", "G", "H"};
    const size_t count = sizeof(names) / sizeof(names[0]);
    for (size_t i = 0; i < count; i++) add_contact(names[i]);

    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 1, &id_manager));
    TEST_ASSERT_NULL(phone_book_find_by_id(head, 1));
    assert_valid_avl(7);

    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 7, &id_manager));
    TEST_ASSERT_NULL(phone_book_find_by_id(head, 7));
    assert_valid_avl(6);

    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 6, &id_manager));
    TEST_ASSERT_NULL(phone_book_find_by_id(head, 6));
    assert_valid_avl(5);

    TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, 2, &id_manager));
    TEST_ASSERT_NULL(phone_book_find_by_id(head, 2));
    assert_valid_avl(4);
}

void test_repeated_removals_should_rebalance_tree(void){
    char last_name[16];
    for (unsigned int i = 1; i <= 63; i++) {
        snprintf(last_name, sizeof(last_name), "%03u", i);
        add_contact(last_name);
    }

    for (unsigned int id = 1; id <= 63; id += 2) {
        TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, id, &id_manager));
        assert_valid_avl(63 - (size_t)((id + 1) / 2));
    }

    for (unsigned int id = 2; id <= 62; id += 2) {
        TEST_ASSERT_EQUAL_INT(0, phone_book_remove(&head, id, &id_manager));
        assert_valid_avl((size_t)(62 - id) / 2);
    }
    TEST_ASSERT_NULL(head);
}

void test_missing_remove_should_not_change_tree(void){
    add_contact("B");
    add_contact("A");
    add_contact("C");
    phone_book_node_t *old_root = head;

    TEST_ASSERT_EQUAL_INT(-1, phone_book_remove(&head, 100, &id_manager));
    TEST_ASSERT_EQUAL_PTR(old_root, head);
    assert_valid_avl(3);
}

void test_edit_last_name_should_reinsert_same_contact(void){
    add_contact("B");
    add_contact("A");
    add_contact("C");

    phone_book_node_t *contact = phone_book_find_by_id(head, 2);
    TEST_ASSERT_EQUAL_INT(0, phone_book_edit_last_name(&head, 2, "Z"));

    TEST_ASSERT_EQUAL_PTR(contact, phone_book_find_by_id(head, 2));
    TEST_ASSERT_EQUAL_STRING("Z", contact->last_name);
    assert_valid_avl(3);
}

void test_invalid_last_name_edit_should_leave_tree_unchanged(void){
    char too_long[NAMESIZE + 1];
    memset(too_long, 'X', NAMESIZE);
    too_long[NAMESIZE] = '\0';
    add_contact("B");

    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_last_name(&head, 1, too_long));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_last_name(&head, 1, NULL));
    TEST_ASSERT_EQUAL_INT(-1, phone_book_edit_last_name(&head, 99, "A"));
    TEST_ASSERT_EQUAL_STRING("B", head->last_name);
    assert_valid_avl(1);
}

void test_clear_should_reset_large_tree(void){
    char last_name[16];
    for (unsigned int i = 0; i < 100; i++) {
        snprintf(last_name, sizeof(last_name), "%03u", i);
        add_contact(last_name);
    }

    phone_book_clear(&head, &id_manager);
    TEST_ASSERT_NULL(head);
    TEST_ASSERT_NULL(id_manager.free_ids);
    TEST_ASSERT_EQUAL_UINT(1, id_manager.next_id);

    phone_book_clear(&head, &id_manager);
    add_contact("Новая");
    TEST_ASSERT_EQUAL_UINT(1, head->id);
    assert_valid_avl(1);
}

int main(void){
    UNITY_BEGIN();

    RUN_TEST(test_insert_should_perform_ll_rotation);
    RUN_TEST(test_insert_should_perform_rr_rotation);
    RUN_TEST(test_insert_should_perform_lr_rotation);
    RUN_TEST(test_insert_should_perform_rl_rotation);
    RUN_TEST(test_sorted_insertions_should_keep_tree_balanced);
    RUN_TEST(test_reverse_insertions_should_keep_tree_balanced);
    RUN_TEST(test_duplicate_last_names_should_not_lose_nodes);
    RUN_TEST(test_remove_leaf_one_child_two_children_and_root);
    RUN_TEST(test_repeated_removals_should_rebalance_tree);
    RUN_TEST(test_missing_remove_should_not_change_tree);
    RUN_TEST(test_edit_last_name_should_reinsert_same_contact);
    RUN_TEST(test_invalid_last_name_edit_should_leave_tree_unchanged);
    RUN_TEST(test_clear_should_reset_large_tree);

    return UNITY_END();
}
