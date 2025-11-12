// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: common symbols for smap drivers
 */

#include "access_tracking_wrapper.h"

#if defined(CONFIG_GUP_GET_PXX_LOW_HIGH) && \
	(defined(CONFIG_SMP) || defined(CONFIG_PREEMPT_RCU))
/*
 * See the comment above ptep_get_lockless() in include/linux/pgtable.h:
 * the barriers in pmdp_get_lockless() cannot guarantee that the value in
 * pmd_high actually belongs with the value in pmd_low; but holding interrupts
 * off blocks the TLB flush between present updates, which guarantees that a
 * successful __pte_offset_map() points to a page from matched halves.
 */
static unsigned long pmdp_get_lockless_start(void)
{
	unsigned long irqflags;

	local_irq_save(irqflags);
	return irqflags;
}
static void pmdp_get_lockless_end(unsigned long irqflags)
{
	local_irq_restore(irqflags);
}
#else
static unsigned long pmdp_get_lockless_start(void)
{
	return 0;
}
static void pmdp_get_lockless_end(unsigned long irqflags)
{
}
#endif

static inline unsigned long *get_pageblock_bitmap(const struct page *p,
						  unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	return section_to_usemap(__pfn_to_section(pfn));
#else
	return page_zone(p)->pageblock_flags;
#endif
}

static inline unsigned long pfn_to_bitidx(const struct page *p,
					  unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	pfn &= (PAGES_PER_SECTION - 1);
#else
	pfn = pfn -
	      round_down(page_zone(p)->zone_start_pfn, pageblock_nr_pages);
#endif
	return (pfn >> pageblock_order) * NR_PAGEBLOCK_BITS;
}

static __always_inline unsigned long
__get_pfnblock_flags_mask(const struct page *page, unsigned long pfn,
			  unsigned long mask)
{
	unsigned long bitidx, word_bitidx;
	unsigned long *bitmap = get_pageblock_bitmap(page, pfn);
	bitidx = pfn_to_bitidx(page, pfn);
	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG - 1);

	return (bitmap[word_bitidx] >> bitidx) & mask;
}

unsigned long get_pfnblock_flags_mask(const struct page *page,
				      unsigned long pfn, unsigned long mask)
{
	return __get_pfnblock_flags_mask(page, pfn, mask);
}

static pte_t *smap__pte_offset_map(pmd_t *pmd, unsigned long addr,
				   pmd_t *pmdvalp)
{
	unsigned long irqflags;
	pmd_t pmdval;

	rcu_read_lock();
	irqflags = pmdp_get_lockless_start();
	pmdval = pmdp_get_lockless(pmd);
	pmdp_get_lockless_end(irqflags);

	if (pmdvalp)
		*pmdvalp = pmdval;
	if (unlikely(pmd_none(pmdval) || is_pmd_migration_entry(pmdval))) {
		goto nomap;
	}
	if (unlikely(pmd_trans_huge(pmdval) || pmd_devmap(pmdval))) {
		goto nomap;
	}
	if (unlikely(pmd_bad(pmdval))) {
		pmd_ERROR(*pmd);
		pmd_clear(pmd);
		goto nomap;
	}
	return __pte_map(&pmdval, addr);
nomap:
	rcu_read_unlock();
	return NULL;
}

pte_t *smap__pte_offset_map_lock(struct mm_struct *mm, pmd_t *pmd,
				 unsigned long addr, spinlock_t **ptlp)
{
	spinlock_t *ptl;
	pmd_t pmdval;
	pte_t *pte;
	while (true) {
		pte = smap__pte_offset_map(pmd, addr, &pmdval);
		if (unlikely(!pte))
			return pte;
		ptl = pte_lockptr(mm, &pmdval);
		spin_lock(ptl);
		if (likely(pmd_same(pmdval, pmdp_get_lockless(pmd)))) {
			*ptlp = ptl;
			return pte;
		}
		pte_unmap_unlock(pte, ptl);
	}
}

static int num_contig_ptes(unsigned long size, size_t *pgsize)
{
	int n_contig_ptes = 0;

	*pgsize = size;

	switch (size) {
#ifndef __PAGETABLE_PMD_FOLDED
	case PUD_SIZE:
		if (pud_sect_supported()) {
			n_contig_ptes = 1;
		}
		break;
#endif
	case PMD_SIZE:
		n_contig_ptes = 1;
		break;
	case CONT_PMD_SIZE:
		*pgsize = PMD_SIZE;
		n_contig_ptes = CONT_PMDS;
		break;
	case CONT_PTE_SIZE:
		*pgsize = PAGE_SIZE;
		n_contig_ptes = CONT_PTES;
		break;
	}

	return n_contig_ptes;
}

pte_t smap_huge_ptep_get(pte_t *ptep)
{
	int ncontig, i;
	size_t pgsize;
	pte_t orig_pte = __ptep_get(ptep);
	if (!pte_present(orig_pte) || !pte_cont(orig_pte))
		return orig_pte;

	ncontig = num_contig_ptes(page_size(pte_page(orig_pte)), &pgsize);
	for (i = 0; i < ncontig; i++, ptep++) {
		pte_t pte = __ptep_get(ptep);
		if (pte_dirty(pte))
			orig_pte = pte_mkdirty(orig_pte);
		if (pte_young(pte))
			orig_pte = pte_mkyoung(orig_pte);
	}
	return orig_pte;
}