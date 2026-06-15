/* SPDX-License-Identifier: MIT */
/*
 * Lightweight KVM layer (variant A): create a VM, register the demo region as
 * a guest memslot, and keep the VM fd open for the process lifetime. That is
 * exactly what SMAP needs to classify us as a guest:
 *
 *   - get_kvm_file_from_task() finds a "kvm-vm" fd in our fd table;
 *   - scan_kvm_gfn() keeps memslots with npages >= 2 MiB and base_gfn >= 1 GiB.
 *
 * No vCPU is created here; host-side accesses to userspace_addr are what drive
 * the pattern. (Variant B - a real vCPU touching the GPA so stage-2 AF bits are
 * set - is a future iteration.)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

#include "memdemo.h"

int vm_register(const struct memdemo_config *cfg,
		const struct mem_region *region, struct vm_context *vm)
{
	vm->kvm_fd = -1;
	vm->vm_fd = -1;

	vm->kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
	if (vm->kvm_fd < 0) {
		perror("open(/dev/kvm)");
		return -1;
	}

	int api = ioctl(vm->kvm_fd, KVM_GET_API_VERSION, 0);
	if (api != KVM_API_VERSION) {
		fprintf(stderr, "unexpected KVM API version %d (want %d)\n", api,
			KVM_API_VERSION);
		goto fail;
	}

	vm->vm_fd = ioctl(vm->kvm_fd, KVM_CREATE_VM, 0);
	if (vm->vm_fd < 0) {
		perror("ioctl(KVM_CREATE_VM)");
		goto fail;
	}

	struct kvm_userspace_memory_region mr;
	memset(&mr, 0, sizeof(mr));
	mr.slot = 0;
	mr.flags = 0;
	mr.guest_phys_addr = cfg->gpa_base;
	mr.memory_size = region->length;
	mr.userspace_addr = (uint64_t)(uintptr_t)region->base;

	if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &mr) < 0) {
		perror("ioctl(KVM_SET_USER_MEMORY_REGION)");
		goto fail;
	}

	fprintf(stderr,
		"vm: memslot registered gpa=0x%llx size=%zu bytes (%llu pages)\n",
		(unsigned long long)cfg->gpa_base, region->length,
		(unsigned long long)region->npages);
	return 0;

fail:
	vm_release(vm);
	return -1;
}

void vm_release(struct vm_context *vm)
{
	if (!vm)
		return;
	if (vm->vm_fd >= 0) {
		close(vm->vm_fd);
		vm->vm_fd = -1;
	}
	if (vm->kvm_fd >= 0) {
		close(vm->kvm_fd);
		vm->kvm_fd = -1;
	}
}
