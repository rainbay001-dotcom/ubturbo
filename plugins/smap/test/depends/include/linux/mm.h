/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <linux/gfp.h>
#include <linux/list.h>
#include <linux/mmzone.h>
#include <linux/mm_types.h>
#include <linux/mmap_lock.h>
#include <linux/page-flags.h>
#include <linux/pfn.h>
#include <linux/page_ref.h>
#include <linux/vmstat.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/huge_mm.h>
#include <uapi/linux/const.h>
#include <asm/page.h>

#define PAGE_SHIFT		12
#define PAGE_SIZE		(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE - 1))

static inline int page_mapcount(struct page *page)
{
	return 0;
}

#define SECTIONS_PGOFF		((sizeof(unsigned long)*8) - SECTIONS_WIDTH)
#define NODES_PGOFF		(SECTIONS_PGOFF - NODES_WIDTH)
#define ZONES_PGOFF		(NODES_PGOFF - ZONES_WIDTH)

#define NODES_PGSHIFT		(NODES_PGOFF * (NODES_WIDTH != 0))
#define ZONES_PGSHIFT		(ZONES_PGOFF * (ZONES_WIDTH != 0))
#define PAGE_ALIGN(addr)		ALIGN(addr, PAGE_SIZE)
#define ZONES_MASK		((1UL << ZONES_WIDTH) - 1)
#define NODES_MASK		((1UL << NODES_WIDTH) - 1)

pte_t *drivers__pte_offset_map(pmd_t *pmd, unsigned long addr, pmd_t *pmdvalp_);
#ifdef __cplusplus
extern "C" {
#endif

int page_to_nid(const struct page *page);

#ifdef __cplusplus
}
#endif

static inline enum zone_type page_zonenum(const struct page *page)
{
	return (enum zone_type)((page->flags >> ZONES_PGSHIFT) & ZONES_MASK);
}

static inline struct zone *page_zone(const struct page *page)
{
	return &NODE_DATA(page_to_nid(page))->node_zones[page_zonenum(page)];
}

static inline unsigned int compound_order(struct page *page)
{
	return page[1].compound_order;
}

static inline unsigned long page_size(struct page *page)
{
	return PAGE_SIZE << compound_order(page);
}

static inline pg_data_t *page_pgdat(const struct page *page)
{
	struct pglist_data *p = NULL;
	return p;
}

static inline int thp_nr_pages(struct page *page)
{
	return 0;
}


static inline void mmgrab(struct mm_struct *mm)
{
}

extern void mmput(struct mm_struct *);

static inline spinlock_t *pmd_lock(struct mm_struct *mm, pmd_t *pmd)
{
    return NULL;
}

#define pte_offset_map_lock(mm, pmd, address, ptlp) (0)

#define pte_unmap_unlock(pte, ptl)	do { \
    spin_unlock(ptl); \
} while (0)

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
static inline spinlock_t *pte_lockptr(struct mm_struct *mm, pmd_t *pmd)
{
	return NULL;
}

static inline struct zone *folio_zone(const struct folio *folio)
{
	return page_zone(&folio->page);
}

static struct vm_area_struct *vma_find(struct vma_iterator *vmi, unsigned long max)
{
	return (struct vm_area_struct *)vmi->mas.tree->ma_root;
}
static inline struct vm_area_struct *vma_next(struct vma_iterator *vmi)
{
	return NULL;
}
static inline bool folio_test_large(struct folio *folio)
{
    return false;
}
static inline unsigned int folio_order(struct folio *folio)
{
	if (!folio_test_large(folio))
		return 0;
	return folio->_flags_1 & 0xff;
}

static inline size_t folio_size(struct folio *folio)
{
    return PAGE_SIZE << folio_order(folio);
}

static inline int folio_nid(const struct folio *folio)
{
    return page_to_nid(&folio->page);
}

static inline unsigned long folio_pfn(struct folio *folio)
{
    return 0;
}

static inline struct folio *pfn_folio(unsigned long pfn)
{
    struct folio *stubFolio;
    if (!pfn) {
        return NULL;
    }
    stubFolio = (struct folio*)malloc(sizeof(*stubFolio));
    return stubFolio;
}

#else /* KERNEL_VERSION(5, 10, 0) */
#endif /* LINUX_VERSION_CODE */


#endif /* _LINUX_MM_H */
