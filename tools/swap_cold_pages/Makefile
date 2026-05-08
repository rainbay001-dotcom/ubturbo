# Kernel source directory — override if headers are elsewhere
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

# Kernel build system objects
obj-m := swap_cold_driver.o
CFLAGS_swap_cold_driver.o := -I$(src)

# Only define top-level targets when invoked directly (not by kbuild)
ifeq ($(KERNELRELEASE),)

all: module userspace

module:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

userspace: swap_cold_pages

swap_cold_pages: swap_cold_pages.c uapi/swap_cold.h
	$(CC) -O2 -Wall -Wextra -I. -o $@ $<

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean
	rm -f swap_cold_pages

.PHONY: all module userspace clean

endif
