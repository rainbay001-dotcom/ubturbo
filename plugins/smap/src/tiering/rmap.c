// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap reverse map module
 */

#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>

#include "smap_migrate_wrapper.h"
#include "ksm.h"
#include "rmap.h"

static unsigned long vma_pgoff_address(pgoff_t pgoff, unsigned long nr_pages,
				       struct vm_area_struct *vma)
{
	unsigned long address;

	if (pgoff >= vma->vm_pgoff) {
		address =
			vma->vm_start + ((pgoff - vma->vm_pgoff) << PAGE_SHIFT);
		/* Check for address beyond vma (or wrapped through 0?) */
		if (address < vma->vm_start || address >= vma->vm_end) {
			address = -EFAULT;
		}
	} else if (pgoff + nr_pages - 1 >= vma->vm_pgoff) {
		/* Test above avoids possibility of wrap to 0 on 32-bit */
		address = vma->vm_start;
	} else {
		address = -EFAULT;
	}
	return address;
}

static inline unsigned long vma_address(struct page *page,
					struct vm_area_struct *vma)
{
	VM_BUG_ON_PAGE(PageKsm(page), page);
	return vma_pgoff_address(page_to_pgoff(page), compound_nr(page), vma);
}

/*
 * Return false if page table scanning in rmap_walk should be stopped.
 * Otherwise, return true.
 */
static bool page_task_one(struct folio *folio, struct vm_area_struct *vma,
			  unsigned long address, void *arg)
{
	struct page_task_arg *pta = arg;
	struct task_struct *task;

	if (!(vma && vma->vm_mm))
		return true;

	rcu_read_lock();
	task = vma->vm_mm->owner;
	rcu_read_unlock();
	if (!task)
		return true;

	if (pta->type == PAGE_PID_TYPE) {
		if (pta->pid == task->pid) {
			pta->found = true;
			return false;
		}
		return true;
	}

	pta->found = true;
	pta->node = cpu_to_node(cpumask_first(&task->cpus_mask));
	pta->nr_cpus_allowed = task->nr_cpus_allowed;

	return false;
}

static struct anon_vma *smap_folio_anon_vma(struct folio *folio)
{
	unsigned long mapping = (unsigned long)folio->mapping;

	if ((mapping & PAGE_MAPPING_FLAGS) != PAGE_MAPPING_ANON)
		return NULL;
	return (void *)(mapping - PAGE_MAPPING_ANON);
}

static struct anon_vma *smap_rmap_walk_anon_lock(struct folio *folio,
						 struct rmap_walk_control *rwc)
{
	struct anon_vma *anon_vma;

	if (rwc->anon_lock) {
		return rwc->anon_lock(folio, rwc);
	}

	/*
	 * Note: remove_migration_ptes() cannot use folio_lock_anon_vma_read()
	 * because that depends on page_mapped(); but not all its usages
	 * are holding mmap_lock. Users without mmap_lock are required to
	 * take a reference count to prevent the anon_vma disappearing
	 */
	anon_vma = smap_folio_anon_vma(folio);
	if (!anon_vma) {
		pr_debug("smap_folio_anon_vma failed.\n");
		return NULL;
	}

	if (anon_vma_trylock_read(anon_vma)) {
		goto out;
	}

	if (rwc->try_lock) {
		anon_vma = NULL;
		rwc->contended = true;
		goto out;
	}

	anon_vma_lock_read(anon_vma);
out:
	return anon_vma;
}

static void smap_rmap_walk_anon(struct folio *folio,
				struct rmap_walk_control *rwc, bool locked)
{
	struct anon_vma *anon_vma;
	pgoff_t pgoff_start, pgoff_end;
	struct anon_vma_chain *anon_vmc;
	if (locked) {
		anon_vma = smap_folio_anon_vma(folio);
		/* anon_vma disappear under us? */
		VM_BUG_ON_FOLIO(!anon_vma, folio);
	} else {
		anon_vma = smap_rmap_walk_anon_lock(folio, rwc);
	}
	if (!anon_vma) {
		return;
	}

	pgoff_start = folio_pgoff(folio);
	pgoff_end = pgoff_start + folio_nr_pages(folio) - 1;
	smap_anon_vma_interval_tree_foreach(anon_vmc, &anon_vma->rb_root,
					    pgoff_start, pgoff_end)
	{
		struct vm_area_struct *vmas = anon_vmc->vma;
		unsigned long vm_address = vma_address(&folio->page, vmas);

		VM_BUG_ON_VMA(vm_address == -EFAULT, vmas);
		cond_resched();

		if (rwc->invalid_vma && rwc->invalid_vma(vmas, rwc->arg)) {
			continue;
		}

		if (!rwc->rmap_one(folio, vmas, vm_address, rwc->arg)) {
			break;
		}
		if (rwc->done && rwc->done(folio)) {
			break;
		}
	}

	if (!locked) {
		anon_vma_unlock_read(anon_vma);
	}
}

static void smap_rmap_walk_file(struct folio *folio,
				struct rmap_walk_control *rwc, bool locked)
{
	struct address_space *f_mapping = folio_mapping(folio);
	pgoff_t pgoff_start, pgoff_end;
	struct vm_area_struct *vmas;

	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	if (!f_mapping) {
		return;
	}

	pgoff_start = folio_pgoff(folio);
	pgoff_end = pgoff_start + folio_nr_pages(folio) - 1;
	if (!locked) {
		if (i_mmap_trylock_read(f_mapping)) {
			goto lookup;
		}

		if (rwc->try_lock) {
			rwc->contended = true;
			return;
		}

		i_mmap_lock_read(f_mapping);
	}
lookup:
	smap_vma_interval_tree_foreach(vmas, &f_mapping->i_mmap, pgoff_start,
				       pgoff_end)
	{
		unsigned long vm_address = vma_address(&folio->page, vmas);

		VM_BUG_ON_VMA(vm_address == -EFAULT, vmas);
		cond_resched();

		if (rwc->invalid_vma && rwc->invalid_vma(vmas, rwc->arg)) {
			continue;
		}

		if (!rwc->rmap_one(folio, vmas, vm_address, rwc->arg)) {
			goto done;
		}
		if (rwc->done && rwc->done(folio)) {
			goto done;
		}
	}

done:
	if (!locked) {
		i_mmap_unlock_read(f_mapping);
	}
}

static void smap_rmap_walk(struct folio *folio, struct rmap_walk_control *rwc)
{
	if (unlikely(folio_test_ksm(folio))) {
		smap_rmap_walk_ksm(folio, rwc);
	} else if (folio_test_anon(folio)) {
		smap_rmap_walk_anon(folio, rwc, false);
	} else {
		smap_rmap_walk_file(folio, rwc, false);
	}
}

static void *smap_page_rmapping(struct page *page)
{
	unsigned long mapping;
	page = compound_head(page);

	mapping = (unsigned long)page->mapping;
	mapping &= ~PAGE_MAPPING_FLAGS;

	return (void *)mapping;
}

void find_page_task(struct page *page, int is_locked, struct page_task_arg *pta)
{
	int we_locked = 0;
	struct rmap_walk_control rwc = {
		.rmap_one = page_task_one,
		.arg = pta,
	};

	if (!smap_page_rmapping(page)) {
		pr_err("page has no mapping\n");
		return;
	}

	if (!is_locked && (!PageAnon(page) || PageKsm(page))) {
		we_locked = trylock_page(page);
		if (!we_locked) {
			return;
		}
	}

	smap_rmap_walk(page_folio(page), &rwc);

	if (we_locked) {
		unlock_page(page);
	}
}
