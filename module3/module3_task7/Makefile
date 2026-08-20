CC := cc
CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -MMD -MP -Iinclude
BUILD_DIR := build
CLIENT_TARGET := $(BUILD_DIR)/client
SERVER_TARGET := $(BUILD_DIR)/server
COMMON_OBJECTS := $(BUILD_DIR)/protocol.o $(BUILD_DIR)/signal_handler.o $(BUILD_DIR)/tcp_socket.o
CLIENT_OBJECTS := $(BUILD_DIR)/client_main.o $(BUILD_DIR)/client.o $(COMMON_OBJECTS)
SERVER_OBJECTS := $(BUILD_DIR)/server_main.o $(BUILD_DIR)/server.o $(COMMON_OBJECTS)
DEPENDENCIES := $(sort $(CLIENT_OBJECTS:.o=.d) $(SERVER_OBJECTS:.o=.d))

.PHONY: all clean test test-chat test-cli test-protocol test-server test-tcp-socket

all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

$(SERVER_TARGET): $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_protocol_encode: tests/test_protocol_encode.c src/protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_tcp_socket: tests/test_tcp_socket.c src/tcp_socket.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_server: tests/test_server.c src/protocol.c src/tcp_socket.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_chat: tests/test_chat.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

test: all test-cli test-protocol test-tcp-socket test-server test-chat

test-chat: all $(BUILD_DIR)/test_chat
	./$(BUILD_DIR)/test_chat ./$(SERVER_TARGET) ./$(CLIENT_TARGET)

test-cli: all
	sh ./tests/test_cli.sh ./$(CLIENT_TARGET) ./$(SERVER_TARGET)

test-protocol: $(BUILD_DIR)/test_protocol_encode
	./$(BUILD_DIR)/test_protocol_encode

test-tcp-socket: $(BUILD_DIR)/test_tcp_socket
	./$(BUILD_DIR)/test_tcp_socket

test-server: all $(BUILD_DIR)/test_server
	./$(BUILD_DIR)/test_server ./$(SERVER_TARGET)

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPENDENCIES)
