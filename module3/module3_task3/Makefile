CC := gcc
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
LDLIBS := -pthread -lrt

TARGET := build/posix_chat
OBJECTS := build/main.o build/chat.o build/message_queue.o build/signal_handler.o
DEPENDS := $(OBJECTS:.o=.d)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDLIBS) -o $@

build/main.o: main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p $@

test: $(TARGET)
	./tests/test_chat.sh ./$(TARGET)

clean:
	rm -rf build

-include $(DEPENDS)
