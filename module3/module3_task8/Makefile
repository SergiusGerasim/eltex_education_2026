CC := cc
CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -MMD -MP -Iinclude
BUILD_DIR := build
TARGET := $(BUILD_DIR)/module3_task8
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/config.o $(BUILD_DIR)/capture.o $(BUILD_DIR)/filter.o $(BUILD_DIR)/output.o $(BUILD_DIR)/packet.o $(BUILD_DIR)/signal_handler.o
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all clean test test-config test-filter test-output test-packet test-signal

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_config: tests/test_config.c src/config.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_signal_handler: tests/test_signal_handler.c src/signal_handler.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_output: tests/test_output.c src/output.c src/config.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_packet: tests/test_packet.c src/packet.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_filter: tests/test_filter.c src/filter.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	$(RM) -r $(BUILD_DIR)

test: test-config test-filter test-output test-packet test-signal

test-config: $(BUILD_DIR)/test_config
	./$(BUILD_DIR)/test_config

test-filter: $(BUILD_DIR)/test_filter
	./$(BUILD_DIR)/test_filter

test-output: $(BUILD_DIR)/test_output
	./$(BUILD_DIR)/test_output

test-packet: $(BUILD_DIR)/test_packet
	./$(BUILD_DIR)/test_packet

test-signal: $(BUILD_DIR)/test_signal_handler
	./$(BUILD_DIR)/test_signal_handler

-include $(DEPENDENCIES)
