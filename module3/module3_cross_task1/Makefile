CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror -Iinclude
BUILD_DIR := build
SERVER := $(BUILD_DIR)/echo_server
CLIENT := $(BUILD_DIR)/echo_client
PROTOCOL_TEST := $(BUILD_DIR)/test_protocol
RAW_UDP_TEST := $(BUILD_DIR)/test_raw_udp
SERVER_STATE_TEST := $(BUILD_DIR)/test_server_state
SIGNAL_HANDLER_TEST := $(BUILD_DIR)/test_signal_handler

.PHONY: all server client test test-protocol test-raw-udp test-server-state test-signal-handler clean

all: server client

server: $(SERVER)

client: $(CLIENT)

test: test-protocol test-raw-udp test-server-state test-signal-handler

test-protocol: $(PROTOCOL_TEST)
	./$(PROTOCOL_TEST)

test-raw-udp: $(RAW_UDP_TEST)
	./$(RAW_UDP_TEST)

test-server-state: $(SERVER_STATE_TEST)
	./$(SERVER_STATE_TEST)

test-signal-handler: $(SIGNAL_HANDLER_TEST)
	./$(SIGNAL_HANDLER_TEST)

$(PROTOCOL_TEST): tests/test_protocol.c src/protocol.c include/protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_protocol.c src/protocol.c -o $@

$(RAW_UDP_TEST): tests/test_raw_udp.c src/raw_udp.c include/raw_udp.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_raw_udp.c src/raw_udp.c -o $@

$(SERVER_STATE_TEST): tests/test_server_state.c src/server.c src/protocol.c src/raw_udp.c src/signal_handler.c include/server.h include/protocol.h include/raw_udp.h include/signal_handler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_server_state.c src/server.c src/protocol.c src/raw_udp.c src/signal_handler.c -o $@

$(SIGNAL_HANDLER_TEST): tests/test_signal_handler.c src/signal_handler.c include/signal_handler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_signal_handler.c src/signal_handler.c -o $@

$(SERVER): main_server.c src/server.c src/protocol.c src/raw_udp.c src/signal_handler.c include/server.h include/protocol.h include/raw_udp.h include/signal_handler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) main_server.c src/server.c src/protocol.c src/raw_udp.c src/signal_handler.c -o $@

$(CLIENT): main_client.c src/client.c src/protocol.c src/raw_udp.c src/signal_handler.c include/client.h include/server.h include/protocol.h include/raw_udp.h include/signal_handler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) main_client.c src/client.c src/protocol.c src/raw_udp.c src/signal_handler.c -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
