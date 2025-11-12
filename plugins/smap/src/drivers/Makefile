KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

obj-m := smap_tracking_core.o
obj-m += smap_access_tracking.o
obj-m += smap_histogram_tracking.o

smap_tracking_core-objs := core.o
smap_tracking_core-objs += bus.o
smap_access_tracking-objs := access_iomem.o access_acpi_helper.o access_acpi_mem.o accessed_bit.o access_ioctl.o access_pid.o access_mmu.o access_tracking.o kvm_pgtable.o access_tracking_wrapper.o memory_notifier.o
smap_histogram_tracking-objs := access_acpi_helper.o access_acpi_mem.o hist_ops.o ub_hist.o smap_hist_mid.o hist_tracking.o

ccflags-y += -DENV_USER -DACPI_NOT_READY

.PHONY: modules clean
.default: modules

modules:
	$(MAKE) -C  $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions module* Module* *.mod *.d *.ko .*.d
