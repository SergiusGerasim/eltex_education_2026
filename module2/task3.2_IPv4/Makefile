CC = gcc

UNITY_DIR = ../libs/Unity

CPPFLAGS = -Iinclude -I$(UNITY_DIR)/src
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow

BUILD_DIR = build
APP = $(BUILD_DIR)/subnet_membership
TEST_APP = $(BUILD_DIR)/test_subnet_membership

APP_SOURCES = main.c src/cheking_subnet_membership.c
TEST_SOURCES = tests/test_subnet_membership.c src/cheking_subnet_membership.c $(UNITY_DIR)/src/unity.c

.PHONY: all test integration-test check clean

all: $(APP)

$(APP): $(APP_SOURCES) include/cheking_subnet_membership.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(APP_SOURCES) -o $@

$(TEST_APP): $(TEST_SOURCES) include/cheking_subnet_membership.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) -o $@

test: $(TEST_APP)
	./$(TEST_APP)

integration-test: $(APP)
	sh tests/test_cli.sh

check: test integration-test

clean:
	$(RM) -r $(BUILD_DIR)
