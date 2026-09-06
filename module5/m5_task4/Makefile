obj-m += chardev_exchange.o

KDIR := /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)
BUILD_DIR := $(CURDIR)/build
MODULE_FILE := $(BUILD_DIR)/chardev_exchange.ko
SIGN_FILE := $(KDIR)/scripts/sign-file
SIGN_KEY_DIR ?= $(HOME)/.local/share/module-signing
SIGN_PRIVATE_KEY ?= $(SIGN_KEY_DIR)/MOK.priv
SIGN_PUBLIC_KEY ?= $(SIGN_KEY_DIR)/MOK.der

.PHONY: all sign clean

all:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) MO=$(BUILD_DIR) modules

sign: all
	@test -f "$(SIGN_PRIVATE_KEY)" || { echo "Private key not found: $(SIGN_PRIVATE_KEY)"; exit 1; }
	@test -f "$(SIGN_PUBLIC_KEY)" || { echo "Public key not found: $(SIGN_PUBLIC_KEY)"; exit 1; }
	$(SIGN_FILE) sha512 $(SIGN_PRIVATE_KEY) $(SIGN_PUBLIC_KEY) $(MODULE_FILE)
	@echo "Signed module: $(MODULE_FILE)"

clean:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) MO=$(BUILD_DIR) clean
