CC = gcc

UNITY_DIR = ../libs/Unity

CPPFLAGS = -Iinclude -I$(UNITY_DIR)/src
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow

BUILD_DIR = build

APP = $(BUILD_DIR)/phone_book
TEST_APP = $(BUILD_DIR)/test_phone_book
AVL_TEST_APP = $(BUILD_DIR)/test_avl_phone_book

APP_SOURCES = main.c src/phone_book.c src/phone_book_ui.c
TEST_SOURCES = tests/test_phone_book.c src/phone_book.c $(UNITY_DIR)/src/unity.c
AVL_TEST_SOURCES = tests/test_avl_phone_book.c src/phone_book.c $(UNITY_DIR)/src/unity.c

.PHONY: all test clean

all: $(APP)

$(APP): $(APP_SOURCES) include/phone_book.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(APP_SOURCES) -o $(APP)

$(TEST_APP): $(TEST_SOURCES) include/phone_book.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) -o $(TEST_APP)

$(AVL_TEST_APP): $(AVL_TEST_SOURCES) include/phone_book.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(AVL_TEST_SOURCES) -o $(AVL_TEST_APP)

test: $(TEST_APP) $(AVL_TEST_APP)
	./$(TEST_APP)
	./$(AVL_TEST_APP)

clean:
	$(RM) -r $(BUILD_DIR)
