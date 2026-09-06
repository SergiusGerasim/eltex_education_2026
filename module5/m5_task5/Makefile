obj-m += netlink_exchange.o

KDIR := /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)
BUILD_DIR := $(CURDIR)/build
MODULE_FILE := $(BUILD_DIR)/netlink_exchange.ko
CLIENT_FILE := $(BUILD_DIR)/netlink_client
SIGN_FILE := $(KDIR)/scripts/sign-file
SIGN_KEY_DIR ?= $(HOME)/.local/share/module-signing
SIGN_PRIVATE_KEY ?= $(SIGN_KEY_DIR)/MOK.priv
SIGN_PUBLIC_KEY ?= $(SIGN_KEY_DIR)/MOK.der
CLIENT_CC ?= gcc
CLIENT_CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2

.PHONY: all module client sign clean

all: module client

module:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) MO=$(BUILD_DIR) modules

client: $(CLIENT_FILE)

$(CLIENT_FILE): netlink_client.c netlink_exchange.h | $(BUILD_DIR)
	$(CLIENT_CC) $(CLIENT_CFLAGS) -o $@ netlink_client.c

$(BUILD_DIR):
	mkdir -p $@

sign: all
	@test -f "$(SIGN_PRIVATE_KEY)" || { echo "Private key not found: $(SIGN_PRIVATE_KEY)"; exit 1; }
	@test -f "$(SIGN_PUBLIC_KEY)" || { echo "Public key not found: $(SIGN_PUBLIC_KEY)"; exit 1; }
	$(SIGN_FILE) sha512 $(SIGN_PRIVATE_KEY) $(SIGN_PUBLIC_KEY) $(MODULE_FILE)
	@echo "Signed module: $(MODULE_FILE)"

clean:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) MO=$(BUILD_DIR) clean
	$(RM) $(CLIENT_FILE)
