#include "unity.h"
#include "priority_queue.h"

#include <string.h>

static PriorityQueue queue;

void setUp(void){
    TEST_ASSERT_TRUE(priority_queue_init(&queue));
}

void tearDown(void){
    priority_queue_free(&queue);
}

void test_init_should_make_all_priority_queues_empty(void){
    for (size_t i = 0; i < PRIORITY_LEVELS; i++){
        TEST_ASSERT_NULL(queue.queue[i].head);
        TEST_ASSERT_NULL(queue.queue[i].tail);
    }
}

void test_push_should_append_in_fifo_order_for_equal_priority(void){
    TEST_ASSERT_TRUE(push(&queue, 42, "first"));
    TEST_ASSERT_TRUE(push(&queue, 42, "second"));

    TEST_ASSERT_EQUAL_STRING("first", queue.queue[42].head->message);
    TEST_ASSERT_EQUAL_STRING("second", queue.queue[42].tail->message);
    TEST_ASSERT_EQUAL_PTR(queue.queue[42].tail, queue.queue[42].head->next);
    TEST_ASSERT_NULL(queue.queue[42].tail->next);
}

void test_push_should_reject_invalid_arguments_and_long_message(void){
    char too_long[MESSAGE_SIZE + 1];
    memset(too_long, 'A', MESSAGE_SIZE);
    too_long[MESSAGE_SIZE] = '\0';

    TEST_ASSERT_FALSE(push(NULL, 0, "message"));
    TEST_ASSERT_FALSE(push(&queue, 0, NULL));
    TEST_ASSERT_FALSE(push(&queue, PRIORITY_LEVELS, "message"));
    TEST_ASSERT_FALSE(push(&queue, 0, too_long));
    TEST_ASSERT_NULL(queue.queue[0].head);
}

void test_peek_should_return_highest_priority_without_removing_it(void){
    char result[MESSAGE_SIZE];
    TEST_ASSERT_TRUE(push(&queue, 10, "low"));
    TEST_ASSERT_TRUE(push(&queue, 200, "high"));

    TEST_ASSERT_TRUE(peek(&queue, result));
    TEST_ASSERT_EQUAL_STRING("high", result);
    TEST_ASSERT_NOT_NULL(queue.queue[200].head);
}

void test_pop_first_should_take_highest_priority_and_preserve_fifo(void){
    char result[MESSAGE_SIZE];
    TEST_ASSERT_TRUE(push(&queue, 100, "middle"));
    TEST_ASSERT_TRUE(push(&queue, 255, "high-first"));
    TEST_ASSERT_TRUE(push(&queue, 255, "high-second"));

    TEST_ASSERT_TRUE(pop_first(&queue, result));
    TEST_ASSERT_EQUAL_STRING("high-first", result);
    TEST_ASSERT_TRUE(pop_first(&queue, result));
    TEST_ASSERT_EQUAL_STRING("high-second", result);
    TEST_ASSERT_TRUE(pop_first(&queue, result));
    TEST_ASSERT_EQUAL_STRING("middle", result);
    TEST_ASSERT_FALSE(pop_first(&queue, result));
}

void test_pop_by_priority_should_only_take_exact_priority(void){
    char result[MESSAGE_SIZE];
    TEST_ASSERT_TRUE(push(&queue, 10, "ten"));
    TEST_ASSERT_TRUE(push(&queue, 20, "twenty"));

    TEST_ASSERT_TRUE(pop_by_priority(&queue, 10, result));
    TEST_ASSERT_EQUAL_STRING("ten", result);
    TEST_ASSERT_FALSE(pop_by_priority(&queue, 10, result));
    TEST_ASSERT_NOT_NULL(queue.queue[20].head);
}

void test_pop_by_priority_or_upper_should_take_highest_matching_priority(void){
    char result[MESSAGE_SIZE];
    TEST_ASSERT_TRUE(push(&queue, 20, "below"));
    TEST_ASSERT_TRUE(push(&queue, 100, "threshold"));
    TEST_ASSERT_TRUE(push(&queue, 150, "above"));
    TEST_ASSERT_TRUE(push(&queue, 250, "highest"));

    TEST_ASSERT_TRUE(pop_by_priority_or_upper(&queue, 100, result));
    TEST_ASSERT_EQUAL_STRING("highest", result);
    TEST_ASSERT_TRUE(pop_by_priority_or_upper(&queue, 100, result));
    TEST_ASSERT_EQUAL_STRING("above", result);
    TEST_ASSERT_TRUE(pop_by_priority_or_upper(&queue, 100, result));
    TEST_ASSERT_EQUAL_STRING("threshold", result);
    TEST_ASSERT_FALSE(pop_by_priority_or_upper(&queue, 100, result));
    TEST_ASSERT_NOT_NULL(queue.queue[20].head);
}

void test_boundary_priorities_should_work(void){
    char result[MESSAGE_SIZE];
    TEST_ASSERT_TRUE(push(&queue, 0, "zero"));
    TEST_ASSERT_TRUE(push(&queue, 255, "maximum"));

    TEST_ASSERT_TRUE(pop_by_priority_or_upper(&queue, 255, result));
    TEST_ASSERT_EQUAL_STRING("maximum", result);
    TEST_ASSERT_TRUE(pop_by_priority_or_upper(&queue, 0, result));
    TEST_ASSERT_EQUAL_STRING("zero", result);
}

void test_empty_queue_and_invalid_arguments_should_fail(void){
    char result[MESSAGE_SIZE];

    TEST_ASSERT_FALSE(peek(&queue, result));
    TEST_ASSERT_FALSE(pop_first(&queue, result));
    TEST_ASSERT_FALSE(pop_by_priority(&queue, 0, result));
    TEST_ASSERT_FALSE(pop_by_priority_or_upper(&queue, 0, result));
    TEST_ASSERT_FALSE(peek(NULL, result));
    TEST_ASSERT_FALSE(peek(&queue, NULL));
    TEST_ASSERT_FALSE(pop_by_priority(&queue, PRIORITY_LEVELS, result));
}

int main(void){
    UNITY_BEGIN();
    RUN_TEST(test_init_should_make_all_priority_queues_empty);
    RUN_TEST(test_push_should_append_in_fifo_order_for_equal_priority);
    RUN_TEST(test_push_should_reject_invalid_arguments_and_long_message);
    RUN_TEST(test_peek_should_return_highest_priority_without_removing_it);
    RUN_TEST(test_pop_first_should_take_highest_priority_and_preserve_fifo);
    RUN_TEST(test_pop_by_priority_should_only_take_exact_priority);
    RUN_TEST(test_pop_by_priority_or_upper_should_take_highest_matching_priority);
    RUN_TEST(test_boundary_priorities_should_work);
    RUN_TEST(test_empty_queue_and_invalid_arguments_should_fail);
    return UNITY_END();
}
