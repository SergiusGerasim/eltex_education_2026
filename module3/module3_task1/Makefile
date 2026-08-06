CC := gcc
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror

TARGET := build/copy_file
OBJECTS := build/main.o build/copy_file.o
DEPENDS := $(OBJECTS:.o=.d)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

build/main.o: main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build/copy_file.o: src/copy_file.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -rf build

test: all
	./tests/test_cli.sh

-include $(DEPENDS)
