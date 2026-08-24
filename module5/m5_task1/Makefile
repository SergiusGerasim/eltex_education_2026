obj-m += hello.o

KDIR := /lib/modules/$(shell uname -r)/build
MODULE_DIR := $(CURDIR)
BUILD_DIR := $(CURDIR)/build

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) MO=$(BUILD_DIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(MODULE_DIR) MO=$(BUILD_DIR) clean

