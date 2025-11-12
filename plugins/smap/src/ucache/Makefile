KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)
obj-m += ucache.o
ucache-y := core.o ucache_migrate.o
 
.PHONY: modules clean
.default: modules
 
modules:
	$(MAKE) -C  $(KERNELDIR) M=$(PWD) modules
 
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean