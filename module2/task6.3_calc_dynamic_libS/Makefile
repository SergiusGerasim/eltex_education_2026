CC = gcc

UNITY_DIR = ../libs/Unity

CPPFLAGS = -Iinclude -I$(UNITY_DIR)/src -DUNITY_INCLUDE_DOUBLE
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow

BUILD_DIR = build
APP = $(BUILD_DIR)/calculator
TEST_APP = $(BUILD_DIR)/test_calculator

APP_SOURCES = main.c src/calculator.c
APP_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(APP_SOURCES))
TEST_SOURCES = tests/test_calculator.c src/calculator.c
TEST_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SOURCES)) $(BUILD_DIR)/unity.o
DEPENDENCIES = $(sort $(APP_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d))

PLUGIN_SOURCES = $(wildcard plugins/*.c)
PLUGINS = $(patsubst plugins/%.c,$(BUILD_DIR)/plugins/%.so,$(PLUGIN_SOURCES))

LDLIBS = -lm -ldl

.PHONY: all run test clean

all: $(APP) $(PLUGINS)

$(APP): $(APP_OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TEST_APP): $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/unity.o: $(UNITY_DIR)/src/unity.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/plugins/%.so: plugins/%.c include/plugin.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -lm -o $@

run: all
	./$(APP)

test: $(TEST_APP) $(PLUGINS)
	./$(TEST_APP) ./$(BUILD_DIR)/plugins

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPENDENCIES)
