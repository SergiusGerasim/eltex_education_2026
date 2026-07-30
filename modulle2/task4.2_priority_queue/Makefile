CC = gcc

UNITY_DIR = ../libs/Unity

CPPFLAGS = -Iinclude -I$(UNITY_DIR)/src
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2

BUILD_DIR = build

APP = $(BUILD_DIR)/priority_queue
TEST_APP = $(BUILD_DIR)/test_priority_queue

APP_SOURCES = main.c src/priority_queue.c
TEST_SOURCES = tests/test_priority_queue.c src/priority_queue.c $(UNITY_DIR)/src/unity.c

.PHONY: all test clean

all: $(APP)

$(APP): $(APP_SOURCES) include/priority_queue.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(APP_SOURCES) -o $(APP)

$(TEST_APP): $(TEST_SOURCES) include/priority_queue.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) -o $(TEST_APP)

test: $(TEST_APP)
	./$(TEST_APP)

clean:
	$(RM) -r $(BUILD_DIR)
