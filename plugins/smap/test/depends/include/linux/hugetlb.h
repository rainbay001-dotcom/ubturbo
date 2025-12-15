/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_HUGETLB_H
#define _LINUX_HUGETLB_H

#include <linux/list.h>
#include <linux/numa.h>
#include <linux/mm_types.h>
#include <linux/cgroup.h>
#include <linux/compiler_attributes.h>
#include <asm/hugetlb.h>
#include <asm/pgtable-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HSTATE_NAME_LEN 32
struct hstate {
    unsigned long free_huge_pages;
    unsigned int demote_order;
    unsigned long nr_huge_pages;
    unsigned int order;
    unsigned long max_huge_pages;
    unsigned long resv_huge_pages;
	unsigned long mask;
    int next_nid_to_alloc;
	int next_nid_to_free;
    unsigned long nr_overcommit_huge_pages;
    unsigned long surplus_huge_pages;
	struct list_head hugepage_activelist;
	struct list_head hugepage_freelists[MAX_NUMNODES];
    unsigned int surplus_huge_pages_node[MAX_NUMNODES];
    unsigned int free_huge_pages_node[MAX_NUMNODES];
    unsigned int nr_huge_pages_node[MAX_NUMNODES];
	unsigned int max_huge_pages_node[MAX_NUMNODES];
	char name[HSTATE_NAME_LEN];
};

typedef bool filter_hugetlb_t(struct folio *folio);
extern int PageHuge(struct page *page);
extern spinlock_t hugetlb_lock;

#ifndef HUGE_MAX_HSTATE
#define HUGE_MAX_HSTATE 1
#endif
extern struct hstate hstates[HUGE_MAX_HSTATE];
#define default_hstate (hstates[default_hstate_idx])
const struct hstate *hugetlb_get_hstate(void);

extern int isolate_hugetlb(struct page *page, struct list_head *list);

extern struct hstate *page_hstate(struct page *page);

extern struct hstate *size_to_hstate(unsigned long size);

#ifndef HUGE_MAX_HSTATE
#define HUGE_MAX_HSTATE 2
#endif

extern struct hstate hstates[HUGE_MAX_HSTATE];
extern unsigned int default_hstate_idx;

#define default_hstate (hstates[default_hstate_idx])

#ifndef MACHINE_HAS_EDAT1
#define MACHINE_HAS_EDAT1 1
#endif

#define hugepages_supported() (MACHINE_HAS_EDAT1)
bool page_huge_active(struct page *page);
void putback_hugetlb_folio(struct folio *folio);

struct hstate *hstate_vma(struct vm_area_struct *vma);
unsigned long huge_page_mask(struct hstate *h);
unsigned long huge_page_size(struct hstate *h);
pte_t huge_ptep_get(pte_t *ptep);
bool is_vm_hugetlb_page(struct vm_area_struct *vma);
struct folio *get_hugetlb_folio_nodemask(unsigned long size, int preferred_nid,
		nodemask_t *nmask, gfp_t gfp_mask, filter_hugetlb_t filter);
static inline void flush_tlb_range(struct vm_area_struct *vma,
                                   unsigned long start, unsigned long end)
{
}
static inline spinlock_t *huge_pte_lockptr(struct hstate *h,
                                           struct mm_struct *mm, pte_t *pte)
{
    return &mm->page_table_lock;
}
static inline spinlock_t *huge_pte_lock(struct hstate *h, struct mm_struct *mm, pte_t *pte)
{
    spinlock_t *ptl_lock;

    ptl_lock = huge_pte_lockptr(h, mm, pte);
    spin_lock(ptl_lock);
    return ptl_lock;
}
#ifdef __cplusplus
}
#endif

#endif
