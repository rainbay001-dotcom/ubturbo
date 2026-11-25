MODULE_NAME := mext_drv
$(MODULE_NAME)-objs := mext_drv_main.o mext_ring_core.o

obj-m := $(MODULE_NAME).o
ccflags-y += -I$(src)/../../src/tiering

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

.PHONY: modules clean
default: modules

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
