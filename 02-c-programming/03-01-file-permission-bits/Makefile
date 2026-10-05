CC = gcc

CPPFLAGS = -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow

BUILD_DIR = build
APP = $(BUILD_DIR)/file_access_rights
SOURCES = main.c src/file_access_rights_via_mask.c

.PHONY: all clean

all: $(APP)

$(APP): $(SOURCES) include/file_access_rights_via_mask.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $(APP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	$(RM) -r $(BUILD_DIR)
