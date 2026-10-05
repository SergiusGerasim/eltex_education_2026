CC = gcc

CPPFLAGS = -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDLIBS = -lm

BUILD_DIR = build
APP = $(BUILD_DIR)/calculator

APP_SOURCES = main.c src/calculator.c
APP_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(APP_SOURCES))
DEPENDENCIES = $(APP_OBJECTS:.o=.d)

.PHONY: all run clean

all: $(APP)

$(APP): $(APP_OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(APP)
	./$(APP)

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPENDENCIES)
