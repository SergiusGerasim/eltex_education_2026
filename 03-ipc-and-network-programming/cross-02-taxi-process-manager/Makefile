CC := gcc
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror -MMD -MP

BUILD_DIR := build
TARGET := $(BUILD_DIR)/taxi_control
PROTOCOL_TEST := $(BUILD_DIR)/test_protocol
TCP_SOCKET_TEST := $(BUILD_DIR)/test_tcp_socket
DRIVER_TEST := $(BUILD_DIR)/test_driver
CLI_TEST := $(BUILD_DIR)/test_cli
APP_TEST := $(BUILD_DIR)/test_app
SIGNAL_HANDLER_TEST := $(BUILD_DIR)/test_signal_handler
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/app.o $(BUILD_DIR)/cli.o $(BUILD_DIR)/driver.o $(BUILD_DIR)/protocol.o $(BUILD_DIR)/signal_handler.o $(BUILD_DIR)/tcp_socket.o
DEPENDS := $(OBJECTS:.o=.d)

.PHONY: all test test-protocol test-tcp-socket test-driver test-cli test-app test-signal-handler clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(PROTOCOL_TEST): tests/test_protocol.c src/protocol.c include/protocol.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_protocol.c src/protocol.c -o $@

$(TCP_SOCKET_TEST): tests/test_tcp_socket.c src/tcp_socket.c include/tcp_socket.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_tcp_socket.c src/tcp_socket.c -o $@

$(DRIVER_TEST): tests/test_driver.c src/driver.c src/protocol.c src/tcp_socket.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_driver.c src/driver.c src/protocol.c src/tcp_socket.c -o $@

$(CLI_TEST): tests/test_cli.c src/cli.c include/cli.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_cli.c src/cli.c -o $@

$(APP_TEST): tests/test_app.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_app.c -o $@

$(SIGNAL_HANDLER_TEST): tests/test_signal_handler.c src/signal_handler.c include/signal_handler.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_signal_handler.c src/signal_handler.c -o $@

test: test-protocol test-tcp-socket test-driver test-cli test-app test-signal-handler

test-protocol: $(PROTOCOL_TEST)
	./$(PROTOCOL_TEST)

test-tcp-socket: $(TCP_SOCKET_TEST)
	./$(TCP_SOCKET_TEST)

test-driver: $(DRIVER_TEST)
	./$(DRIVER_TEST)

test-cli: $(CLI_TEST)
	./$(CLI_TEST)

test-app: all $(APP_TEST)
	./$(APP_TEST) ./$(TARGET)

test-signal-handler: $(SIGNAL_HANDLER_TEST)
	./$(SIGNAL_HANDLER_TEST)

$(BUILD_DIR):
	mkdir -p $@

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPENDS)
