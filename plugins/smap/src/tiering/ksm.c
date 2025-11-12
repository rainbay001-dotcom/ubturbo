// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap reverse map module
 */

#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/ksm.h>

#include "ksm.h"

static inline unsigned long vma_start_pgoff(struct vm_area_struct *v)
{
	return v->vm_pgoff;
}

static inline unsigned long vma_last_pgoff(struct vm_area_struct *v)
{
	return v->vm_pgoff + vma_pages(v) - 1;
}

static inline unsigned long avc_start_pgoff(struct anon_vma_chain *avc)
{
	return vma_start_pgoff(avc->vma);
}

static inline unsigned long avc_last_pgoff(struct anon_vma_chain *avc)
{
	return vma_last_pgoff(avc->vma);
}

INTERVAL_TREE_DEFINE(struct anon_vma_chain, rb, unsigned long, rb_subtree_last,
		     avc_start_pgoff, avc_last_pgoff,
		     /* empty */, smap_anon_vma_interval_tree)

INTERVAL_TREE_DEFINE(struct vm_area_struct, shared.rb, unsigned long,
		     shared.rb_subtree_last, vma_start_pgoff, vma_last_pgoff,
		     /* empty */, smap_vma_interval_tree)

static inline void *folio_raw_mapping(struct folio *folio)
{
	unsigned long mapping = (unsigned long)folio->mapping;

	return (void *)(mapping & ~PAGE_MAPPING_FLAGS);
}

static inline struct ksm_stable_node *folio_stable_node(struct folio *folio)
{
	return folio_test_ksm(folio) ? folio_raw_mapping(folio) : NULL;
}

void smap_rmap_walk_ksm(struct folio *folio, struct rmap_walk_control *rwc)
{
	struct ksm_stable_node *k_stable_node;
	struct ksm_rmap_item *k_rmap_item;
	int search_new_forks = 0;

	VM_BUG_ON_FOLIO(!folio_test_ksm(folio), folio);

	/*
	 * Rely on the page lock to protect against concurrent modifications
	 * to that page's node of the stable tree.
	 */
	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	k_stable_node = folio_stable_node(folio);
	if (!k_stable_node)
		return;
again:
	hlist_for_each_entry(k_rmap_item, &k_stable_node->hlist, hlist) {
		struct anon_vma *anon_vma = k_rmap_item->anon_vma;
		struct anon_vma_chain *anon_vmac;
		struct vm_area_struct *vmas;

		cond_resched();
		if (!anon_vma_trylock_read(anon_vma)) {
			if (rwc->try_lock) {
				rwc->contended = true;
				return;
			}
			anon_vma_lock_read(anon_vma);
		}
		smap_anon_vma_interval_tree_foreach(
			anon_vmac, &anon_vma->rb_root, 0, ULONG_MAX)
		{
			unsigned long addr;

			cond_resched();
			vmas = anon_vmac->vma;

			/* Ignore the stable/unstable/sqnr flags */
			addr = k_rmap_item->address & PAGE_MASK;

			if (addr < vmas->vm_start || addr >= vmas->vm_end)
				continue;
			/*
			 * Initially we examine only the vma which covers this
			 * k_rmap_item; but later, if there is still work to do,
			 * we examine covering vmas in other mms: in case they
			 * were forked from the original since ksmd passed.
			 */
			if ((k_rmap_item->mm == vmas->vm_mm) ==
			    search_new_forks)
				continue;

			if (rwc->invalid_vma &&
			    rwc->invalid_vma(vmas, rwc->arg))
				continue;

			if (!rwc->rmap_one(folio, vmas, addr, rwc->arg)) {
				anon_vma_unlock_read(anon_vma);
				return;
			}
			if (rwc->done && rwc->done(folio)) {
				anon_vma_unlock_read(anon_vma);
				return;
			}
		}
		anon_vma_unlock_read(anon_vma);
	}
	if (!search_new_forks++)
		goto again;
}