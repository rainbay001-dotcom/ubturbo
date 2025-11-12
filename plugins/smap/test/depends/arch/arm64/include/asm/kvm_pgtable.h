/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: kvm_pgtable.h
*/
#ifndef __ARM64_KVM_PGTABLE_H__
#define __ARM64_KVM_PGTABLE_H__

#include <asm/pgtable-hwdef.h>
#include <linux/kvm_host.h>
#include <linux/types.h>
#include <linux/pfn.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/bug.h>

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define EINVAL 22
#define ERANGE 34
#define PTRS_PER_PTE (1UL << (PAGE_SHIFT - 3))
#define BIT(x) x

#define FIELD_GET(_mask, _reg) ((_mask) & (_reg))
#define GENMASK(h, l) ((l) | (h))
#define __va(x) (x)
#define ALIGN_DOWN(x, align_to) 0
#define WARN_ON_ONCE(condition) 1
#define WRITE_ONCE(x, val) 1
#define KVM_PGTABLE_MAX_LEVELS 4U

typedef u64 kvm_pte_t;

typedef kvm_pte_t *kvm_pteref_t;
struct kvm_pgtable {
	u32					ia_bits;
	u32					start_level;
	kvm_pteref_t				pgd;
	struct kvm_pgtable_mm_ops *mm_ops;
};


enum kvm_pgtable_walk_flags {
	KVM_PGTABLE_WALK_LEAF			= BIT(0),
	KVM_PGTABLE_WALK_TABLE_PRE		= BIT(1),
	KVM_PGTABLE_WALK_TABLE_POST		= BIT(2),
	KVM_PGTABLE_WALK_SHARED			= BIT(3),
	KVM_PGTABLE_WALK_HANDLE_FAULT		= BIT(4),
	KVM_PGTABLE_WALK_SKIP_BBM_TLBI		= BIT(5),
	KVM_PGTABLE_WALK_SKIP_CMO		= BIT(6),
};

struct kvm_pgtable_mm_ops {
	void* (*zalloc_page)(void *arg);
	void* (*zalloc_pages_exact)(size_t size);
	void (*free_pages_exact)(void *addr, size_t size);
	void (*free_unlinked_table)(void *addr, u32 level);
	int (*page_count)(void *addr);
    void (*put_page)(void *addr);
	void (*get_page)(void *addr);
	void* (*phys_to_virt)(phys_addr_t phys);
	phys_addr_t (*virt_to_phys)(void *addr);
	void (*icache_inval_pou)(void *addr, size_t size);
    void (*dcache_clean_inval_poc)(void *addr, size_t size);
};

struct kvm_pgtable_visit_ctx {
	kvm_pte_t *ptep;
	kvm_pte_t old;
	void *arg;
	struct kvm_pgtable_mm_ops *mm_ops;
	u64 start;
	u64 addr;
	u64 end;
	u32 level;
	enum kvm_pgtable_walk_flags flags;
};

typedef int (*kvm_pgtable_visitor_fn_t)(const struct kvm_pgtable_visit_ctx *ctx,
					enum kvm_pgtable_walk_flags visit);

static inline bool kvm_pgtable_walk_shared(const struct kvm_pgtable_visit_ctx *ctx)
{
	return ctx->flags & KVM_PGTABLE_WALK_SHARED;
}

struct kvm_pgtable_walker {
	const kvm_pgtable_visitor_fn_t cb;
	void* const arg;
	const enum kvm_pgtable_walk_flags flags;
};

static inline u64 kvm_granule_shift(u32 level)
{
	/* Assumes KVM_PGTABLE_MAX_LEVELS is 4 */
	return 0;
}

static inline u64 kvm_granule_size(u32 level)
{
	return 0;
}

static inline int kvm_pgtable_walk_begin(struct kvm_pgtable_walker *walker)
{
	return 0;
}
static inline void kvm_pgtable_walk_end(struct kvm_pgtable_walker *walker)
{
}

#define KVM_PTE_VALID			BIT(0)

static inline bool kvm_pte_valid(kvm_pte_t pte)
{
	return pte & KVM_PTE_VALID;
}
static inline u64 kvm_pte_to_phys(kvm_pte_t pte)
{
	return 0;
}
static inline kvm_pte_t *kvm_dereference_pteref(struct kvm_pgtable_walker *walker,
						kvm_pteref_t pteref)
{
	return pteref;
}

#define cmpxchg(a, b, c) (b)

#else /* KERNEL_VERSION(5, 10, 0) */

#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define EINVAL 22
#define ERANGE 34
#define PTRS_PER_PTE (1UL << (PAGE_SHIFT - 3))
#define BIT(x) (1 << (x))

#define FIELD_GET(_mask, _reg) ((_mask) & (_reg))
#define GENMASK(h, l) ((l) | (h))
#define __va(x) (x)
#define ALIGN_DOWN(x, align_to) 0
#define WARN_ON_ONCE(condition) 1
#define WRITE_ONCE(x, val) 1

typedef u64 kvm_pte_t;

struct kvm_pgtable {
	u32 ia_bits;
	u32 start_level;
	kvm_pte_t *pgd;

	/* Stage-2 only */
	struct kvm_s2_mmu *mmu;
};

enum kvm_pgtable_walk_flags {
	KVM_PGTABLE_WALK_LEAF = BIT(0),
	KVM_PGTABLE_WALK_TABLE_PRE = BIT(1),
	KVM_PGTABLE_WALK_TABLE_POST = BIT(2),
};

typedef int (*kvm_pgtable_visitor_fn_t)(u64 addr, u64 end, u32 level,
			 kvm_pte_t *ptep, enum kvm_pgtable_walk_flags flag, void *const arg);

struct kvm_pgtable_walker {
	const kvm_pgtable_visitor_fn_t cb;
	void *const arg;
	const enum kvm_pgtable_walk_flags flags;
};

#endif /* KERNEL_VERSION(6, 6, 0) */
#endif /* __ARM64_KVM_PGTABLE_H__ */
