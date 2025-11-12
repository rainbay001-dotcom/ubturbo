/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __KVM_HOST_H
#define __KVM_HOST_H

#include <asm/kvm_pgtable.h>

#include <linux/kvm_types.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/srcutree.h>

#define KVM_ADDRESS_SPACE_NUM 1

typedef u64 gfn_t;

typedef u64 kvm_pte_t;

struct kvm_s2_mmu {
	struct kvm_pgtable *pgt;
};

struct kvm_arch {
	struct kvm_s2_mmu mmu;
};

struct kvm_memory_slot {
	gfn_t base_gfn;
	unsigned long npages;
	unsigned long userspace_addr;
};

struct kvm_memslots {
	int used_slots;
	struct kvm_memory_slot *memslots;
};

static inline gfn_t gpa_to_gfn(gpa_t gpa)
{
	return (gfn_t)(gpa >> PAGE_SHIFT);
}

struct kvm {
	spinlock_t mmu_lock;
	struct mutex slots_lock;
	struct mm_struct *mm; /* userspace tied to this vm */
	struct kvm_memslots __rcu *memslots[KVM_ADDRESS_SPACE_NUM];
	struct srcu_struct srcu;
	struct kvm_arch arch;
};

#ifdef __cplusplus
extern "C" {
#endif

unsigned long gfn_to_hva_memslot(struct kvm_memory_slot *slot, gfn_t gfn);
struct kvm_memslots *kvm_memslots(struct kvm *kvm);
void kvm_flush_remote_tlbs(struct kvm *kvm);
int srcu_read_lock(struct srcu_struct *ssp);
void srcu_read_unlock(struct srcu_struct *ssp, int idx);

#ifdef __cplusplus
}
#endif

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
#define kvm_for_each_memslot(memslot, bkt, slots)                                   \
	for ((memslot) = &(slots)->memslots[0];                                    \
	     (memslot) < (slots)->memslots + (slots)->used_slots; (memslot)++)         \
		if ((!(memslot)->npages)) {                                      \
		} else
#else
#define kvm_for_each_memslot(memslot, slots)                                   \
	for ((memslot) = &(slots)->memslots[0];                                    \
	     (memslot) < (slots)->memslots + (slots)->used_slots; (memslot)++)         \
		if ((!(memslot)->npages)) {                                      \
		} else
#endif
#endif
