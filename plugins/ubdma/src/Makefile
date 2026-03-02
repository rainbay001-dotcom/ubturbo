MODULE_NAME = ub_dma
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD		  := $(shell pwd)
ccflags-y +=  -I/usr/src/kernels/$(KERNELRELEASE)/include/ub/urma
ccflags-y += -fstack-protector-strong

obj-m := $(MODULE_NAME).o

$(MODULE_NAME)-objs := urma.o ub_dma_main.o ub_virt_dma.o segment_manager.o memory_notify.o ub_dma_iomem.o

.PHONY: modules
default: modules

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules KBUILD_MODPOST_WARN=1

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean