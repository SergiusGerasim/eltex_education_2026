CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror -Iinclude
BUILD_DIR := build
PROTOCOL_TEST := $(BUILD_DIR)/test_protocol

.PHONY: test-protocol clean

test-protocol: $(PROTOCOL_TEST)
	./$(PROTOCOL_TEST)

$(PROTOCOL_TEST): tests/test_protocol.c src/protocol.c include/protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_protocol.c src/protocol.c -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
