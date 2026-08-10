CC := gcc
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror

TARGET := build/broker_publ_sub
OBJECTS := build/main.o build/broker.o build/publisher.o build/subscriber.o
DEPENDS := $(OBJECTS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

build/main.o: main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -rf build

-include $(DEPENDS)
