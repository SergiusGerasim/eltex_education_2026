CC := cc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -MMD -MP -Iinclude
BUILD_DIR := build
TARGET := $(BUILD_DIR)/module3_task4
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/ipc.o $(BUILD_DIR)/producer.o $(BUILD_DIR)/consumer.o
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPENDENCIES)
