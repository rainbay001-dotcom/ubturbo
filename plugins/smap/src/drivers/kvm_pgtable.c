// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: kvm_pgtable functions
 */

#include <asm/stage2_pgtable.h>
#include <asm/kvm_pgtable.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/pagewalk.h>
#include <linux/kvm_host.h>
#include <linux/fdtable.h>
#include <linux/rmap.h>
#include <linux/vmalloc.h>
#include <linux/sort.h>
#include <linux/hugetlb.h>
#include <linux/mmzone.h>

#include "kvm_pgtable.h"

struct kvm_pgtable_walk_data {
	struct kvm_pgtable_walker *walker;

	const u64 start;
	u64 addr;
	const u64 end;
};

#define KVM_PTE_LEAF_ATTR_LO_S2_AF BIT(10)
#define KVM_PTE_VALID BIT(0)
#define KVM_PTE_TYPE BIT(1)
#define KVM_PTE_TYPE_BLOCK 0
#define KVM_PTE_TYPE_PAGE 1
#define KVM_PTE_TYPE_TABLE 1
#define KVM_PTE_LEAF_ATTR_LO GENMASK(11, 2)
#define KVM_PTE_LEAF_ATTR_HI GENMASK(63, 50)
#define KVM_PTE_LEAF_ATTR_HI_S2_XN BIT(54)

static u32 kvm_pgtable_idx(struct kvm_pgtable_walk_data *data, u32 level)
{
	u64 shift = kvm_granule_shift(level);
	u64 mask = BIT(PAGE_SHIFT - 3) - 1;

	return (data->addr >> shift) & mask;
}

static bool kvm_pte_table(kvm_pte_t pte, u32 level)
{
	if (level == KVM_PGTABLE_MAX_LEVELS - 1 || !kvm_pte_valid(pte))
		return false;

	return FIELD_GET(KVM_PTE_TYPE, pte) == KVM_PTE_TYPE_TABLE;
}

static int kvm_pgtable_visitor_cb(struct kvm_pgtable_walk_data *data,
				  const struct kvm_pgtable_visit_ctx *ctx,
				  enum kvm_pgtable_walk_flags visit)
{
	struct kvm_pgtable_walker *walker = data->walker;

	/* Ensure the appropriate lock is held (e.g. RCU lock for stage-2 MMU) */
	WARN_ON_ONCE(kvm_pgtable_walk_shared(ctx) &&
		     !kvm_pgtable_walk_lock_held());
	return walker->cb(ctx, visit);
}
static bool kvm_pgtable_walk_continue(const struct kvm_pgtable_walker *walker,
				      int r)
{
	/*
	 * Visitor callbacks return EAGAIN when the conditions that led to a
	 * fault are no longer reflected in the page tables due to a race to
	 * update a PTE. In the context of a fault handler this is interpreted
	 * as a signal to retry guest execution.
	 *
	 * Ignore the return code altogether for walkers outside a fault handler
	 * (e.g. write protecting a range of memory) and chug along with the
	 * page table walk.
	 */
	if (r == -EAGAIN)
		return !(walker->flags & KVM_PGTABLE_WALK_HANDLE_FAULT);

	return !r;
}

static kvm_pte_t *kvm_pte_follow(kvm_pte_t pte,
				 struct kvm_pgtable_mm_ops *mm_ops)
{
	return mm_ops->phys_to_virt(kvm_pte_to_phys(pte));
}

static int __kvm_pgtable_walk(struct kvm_pgtable_walk_data *data,
			      struct kvm_pgtable_mm_ops *mm_ops,
			      kvm_pteref_t pgtable, u32 level);

static inline int __kvm_pgtable_visit(struct kvm_pgtable_walk_data *data,
				      struct kvm_pgtable_mm_ops *mm_ops,
				      kvm_pteref_t pteref, u32 level)
{
	enum kvm_pgtable_walk_flags flags = data->walker->flags;
	kvm_pte_t *ptep = kvm_dereference_pteref(data->walker, pteref);
	struct kvm_pgtable_visit_ctx ctx = {
		.ptep = ptep,
		.old = READ_ONCE(*ptep),
		.arg = data->walker->arg,
		.mm_ops = mm_ops,
		.start = data->start,
		.addr = data->addr,
		.end = data->end,
		.level = level,
		.flags = flags,
	};
	int ret = 0;
	bool reload = false;
	kvm_pteref_t childp;
	bool table = kvm_pte_table(ctx.old, level);
	if (table && (ctx.flags & KVM_PGTABLE_WALK_TABLE_PRE)) {
		ret = kvm_pgtable_visitor_cb(data, &ctx,
					     KVM_PGTABLE_WALK_TABLE_PRE);
		reload = true;
	}

	if (!table && (ctx.flags & KVM_PGTABLE_WALK_LEAF)) {
		ret = kvm_pgtable_visitor_cb(data, &ctx, KVM_PGTABLE_WALK_LEAF);
		reload = true;
	}
	/*
	 * Reload the page table after invoking the walker callback for leaf
	 * entries or after pre-order traversal, to allow the walker to descend
	 * into a newly installed or replaced table.
	 */
	if (reload) {
		ctx.old = READ_ONCE(*ptep);
		table = kvm_pte_table(ctx.old, level);
	}
	if (!kvm_pgtable_walk_continue(data->walker, ret)) {
		goto out;
	}
	if (!table) {
		data->addr = ALIGN_DOWN(data->addr, kvm_granule_size(level));
		data->addr += kvm_granule_size(level);
		goto out;
	}

	childp = (kvm_pteref_t)kvm_pte_follow(ctx.old, mm_ops);
	ret = __kvm_pgtable_walk(data, mm_ops, childp, level + 1);
	if (!kvm_pgtable_walk_continue(data->walker, ret)) {
		goto out;
	}
	if (ctx.flags & KVM_PGTABLE_WALK_TABLE_POST)
		ret = kvm_pgtable_visitor_cb(data, &ctx,
					     KVM_PGTABLE_WALK_TABLE_POST);

out:
	if (kvm_pgtable_walk_continue(data->walker, ret))
		return 0;

	return ret;
}

static int __kvm_pgtable_walk(struct kvm_pgtable_walk_data *data,
			      struct kvm_pgtable_mm_ops *mm_ops,
			      kvm_pteref_t pgtable, u32 level)
{
	u32 idx;
	int ret = 0;

	if (WARN_ON_ONCE(level >= KVM_PGTABLE_MAX_LEVELS))
		return -EINVAL;

	for (idx = kvm_pgtable_idx(data, level); idx < PTRS_PER_PTE; ++idx) {
		kvm_pteref_t pteref = &pgtable[idx];

		if (data->addr >= data->end)
			break;

		ret = __kvm_pgtable_visit(data, mm_ops, pteref, level);
		if (ret)
			break;
	}

	return ret;
}

static u32 kvm_pgd_page_idx(struct kvm_pgtable *pgt, u64 addr)
{
	u64 shift = kvm_granule_shift(pgt->start_level - 1); /* May underflow */
	u64 mask = BIT(pgt->ia_bits) - 1;

	return (addr & mask) >> shift;
}

static int _kvm_pgtable_walk(struct kvm_pgtable *pgt,
			     struct kvm_pgtable_walk_data *data)
{
	u32 idx;
	int ret = 0;
	u64 limit = BIT(pgt->ia_bits);
	if (data->addr > limit || data->end > limit)
		return -ERANGE;

	if (!pgt->pgd)
		return -EINVAL;

	for (idx = kvm_pgd_page_idx(pgt, data->addr); data->addr < data->end;
	     ++idx) {
		kvm_pteref_t pteref = &pgt->pgd[idx * PTRS_PER_PTE];

		ret = __kvm_pgtable_walk(data, pgt->mm_ops, pteref,
					 pgt->start_level);
		if (ret)
			break;
	}

	return ret;
}

int kvm_pgtable_walk(struct kvm_pgtable *pgt, u64 addr, u64 size,
		     struct kvm_pgtable_walker *walker)
{
	struct kvm_pgtable_walk_data walk_data = {
		.start = ALIGN_DOWN(addr, PAGE_SIZE),
		.addr = ALIGN_DOWN(addr, PAGE_SIZE),
		.end = PAGE_ALIGN(walk_data.addr + size),
		.walker = walker,
	};
	int r;

	r = kvm_pgtable_walk_begin(walker);
	if (r) {
		return r;
	}

	r = _kvm_pgtable_walk(pgt, &walk_data);
	kvm_pgtable_walk_end(walker);

	return r;
}

static bool stage2_try_set_pte(const struct kvm_pgtable_visit_ctx *ctx,
			       kvm_pte_t new)
{
	if (!kvm_pgtable_walk_shared(ctx)) {
		WRITE_ONCE(*ctx->ptep, new);
		return true;
	}

	return cmpxchg(ctx->ptep, ctx->old, new) == ctx->old;
}

struct smap_stage2_test_clear_young_data {
	kvm_pte_t attr_set;
	kvm_pte_t attr_clr;
	bool mkold;
	kvm_pte_t pte;
	u32 level;
	bool young;
};

static inline bool stage2_pte_executable(kvm_pte_t pte)
{
	return !(pte & KVM_PTE_LEAF_ATTR_HI_S2_XN);
}

static int smap_stage2_test_clear_young_walker(const struct kvm_pgtable_visit_ctx *ctx,
				    enum kvm_pgtable_walk_flags visit)
{
	kvm_pte_t pte = ctx->old;
	kvm_pte_t new = ctx->old & ~KVM_PTE_LEAF_ATTR_LO_S2_AF;
	struct smap_stage2_test_clear_young_data *data = ctx->arg;
	struct kvm_pgtable_mm_ops *mm_ops = ctx->mm_ops;

	if (!kvm_pte_valid(ctx->old) || new == ctx->old)
		return 0;

	data->young = true;

	if (!data->mkold)
		return 0;

	data->level = ctx->level;
	data->pte = pte;
	pte &= ~data->attr_clr;
	pte |= data->attr_set;

	if (data->pte != pte) {
		if (mm_ops->icache_inval_pou && stage2_pte_executable(pte) &&
		    !stage2_pte_executable(ctx->old)) {
			mm_ops->icache_inval_pou(kvm_pte_follow(pte, mm_ops),
						 kvm_granule_size(ctx->level));
		}

		if (!stage2_try_set_pte(ctx, pte))
			return -EAGAIN;
	}

	return 0;
}

static int smap_stage2_test_clear_young(struct kvm_pgtable *pgt, u64 addr,
					u64 size, kvm_pte_t attr_set,
					kvm_pte_t attr_clr, bool mkold,
					kvm_pte_t *orig_pte, int *level,
					bool *young,
					enum kvm_pgtable_walk_flags flags)
{
	int ret;
	kvm_pte_t attr_mask = KVM_PTE_LEAF_ATTR_LO | KVM_PTE_LEAF_ATTR_HI;
	struct smap_stage2_test_clear_young_data data = {
		.attr_set = attr_set & attr_mask,
		.attr_clr = attr_clr & attr_mask,
		.mkold = mkold,
	};
	struct kvm_pgtable_walker walker = {
		.cb = smap_stage2_test_clear_young_walker,
		.arg = &data,
		.flags = flags | KVM_PGTABLE_WALK_LEAF,
	};

	ret = kvm_pgtable_walk(pgt, addr, size, &walker);
	if (ret)
		return ret;

	if (young)
		*young = data.young;

	if (level)
		*level = data.level;

	if (orig_pte)
		*orig_pte = data.pte;

	return 0;
}

bool smap_kvm_pgtable_stage2_test_clear_young(struct kvm_pgtable *pgt, u64 addr,
					      bool mkold)
{
	kvm_pte_t pte = 0;
	int ret;
	bool young = false;

	ret = smap_stage2_test_clear_young(
		pgt, addr, 1, 0, KVM_PTE_LEAF_ATTR_LO_S2_AF, mkold, &pte, NULL,
		&young,
		KVM_PGTABLE_WALK_HANDLE_FAULT | KVM_PGTABLE_WALK_SHARED);
	if (!ret)
		dsb(ishst);
	return young;
}

bool smap_kvm_pgtable_stage2_mkold(struct kvm_pgtable *pgt, u64 addr)
{
	return smap_kvm_pgtable_stage2_test_clear_young(pgt, addr, true);
}

bool smap_kvm_pgtable_stage2_is_young(struct kvm_pgtable *pgt, u64 addr)
{
	return smap_kvm_pgtable_stage2_test_clear_young(pgt, addr, false);
}
