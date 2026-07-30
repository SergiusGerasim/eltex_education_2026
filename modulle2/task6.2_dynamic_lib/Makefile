CC = gcc

UNITY_DIR = ../libs/Unity

CPPFLAGS = -Iinclude -I$(UNITY_DIR)/src
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow

BUILD_DIR = build

APP = $(BUILD_DIR)/phone_book
TEST_APP = $(BUILD_DIR)/test_phone_book
LIBRARY = $(BUILD_DIR)/libphonebook.so

LIB_OBJECT = $(BUILD_DIR)/phone_book.o
APP_OBJECTS = $(BUILD_DIR)/main.o $(BUILD_DIR)/phone_book_ui.o
TEST_OBJECTS = $(BUILD_DIR)/test_phone_book.o $(BUILD_DIR)/unity.o

.PHONY: all test clean

all: $(APP)

$(APP): $(APP_OBJECTS) $(LIBRARY)
	$(CC) $(APP_OBJECTS) -L$(BUILD_DIR) -lphonebook -Wl,-rpath,'$$ORIGIN' -o $@

$(LIBRARY): $(LIB_OBJECT)
	$(CC) -shared $^ -o $@

$(BUILD_DIR)/phone_book.o: src/phone_book.c include/phone_book.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

$(BUILD_DIR)/phone_book_ui.o: src/phone_book_ui.c include/phone_book_ui.h include/phone_book.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: main.c include/phone_book_ui.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_phone_book.o: tests/test_phone_book.c include/phone_book.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/unity.o: $(UNITY_DIR)/src/unity.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_APP): $(TEST_OBJECTS) $(LIBRARY)
	$(CC) $(TEST_OBJECTS) -L$(BUILD_DIR) -lphonebook -Wl,-rpath,'$$ORIGIN' -o $@

test: $(TEST_APP)
	./$(TEST_APP)

clean:
	$(RM) -r $(BUILD_DIR)
