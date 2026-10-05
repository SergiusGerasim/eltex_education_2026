CC := cc
CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -MMD -MP -Iinclude
BUILD_DIR := build
TARGET := $(BUILD_DIR)/module3_task6
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/chat.o $(BUILD_DIR)/protocol.o $(BUILD_DIR)/udp_socket.o $(BUILD_DIR)/signal_handler.o
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all clean test test-chat test-protocol

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_udp_socket: tests/test_udp_socket.c src/udp_socket.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_protocol_encode: tests/test_protocol_encode.c src/protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	$(RM) -r $(BUILD_DIR)

test: $(TARGET) $(BUILD_DIR)/test_udp_socket $(BUILD_DIR)/test_protocol_encode
	./$(BUILD_DIR)/test_udp_socket
	./$(BUILD_DIR)/test_protocol_encode
	sh ./tests/test_chat.sh ./$(TARGET)

test-chat: $(TARGET)
	sh ./tests/test_chat.sh ./$(TARGET)

test-protocol: $(BUILD_DIR)/test_protocol_encode
	./$(BUILD_DIR)/test_protocol_encode

-include $(DEPENDENCIES)
