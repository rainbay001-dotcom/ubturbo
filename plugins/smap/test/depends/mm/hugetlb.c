// SPDX-License-Identifier: GPL-2.0-only
#include <linux/mm.h>
#include <linux/migrate.h>
#include <asm/pgtable.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hstate hstates[HUGE_MAX_HSTATE];
spinlock_t hugetlb_lock;

int PageHuge(struct page *page)
{
    return 0;
}


int PageHead(struct page *page)
{
    return 0;
}

struct hstate *hstate_vma(struct vm_area_struct *vma)
{
    return NULL;
}

unsigned long huge_page_mask(struct hstate *h)
{
    return 0;
}

unsigned long huge_page_size(struct hstate *h)
{
    return 0;
}

pte_t huge_ptep_get(pte_t *ptep)
{
    return *ptep;
}

bool is_vm_hugetlb_page(struct vm_area_struct *vma)
{
    return 0;
}

const struct hstate *hugetlb_get_hstate(void)
{
    return &default_hstate;
}

int isolate_hugetlb(struct page *page, struct list_head *list)
{
    return 0;
}

struct hstate *page_hstate(struct page *page)
{
    struct hstate *h = NULL;
    return h;
}

struct hstate *size_to_hstate(unsigned long size)
{
    return NULL;
}

unsigned int default_hstate_idx;
struct hstate hstates[HUGE_MAX_HSTATE];

bool page_huge_active(struct page *page)
{
    return true;
}

struct folio *get_hugetlb_folio_nodemask(unsigned long size, int preferred_nid,
        nodemask_t *nmask, gfp_t gfp_mask, filter_hugetlb_t filter)
{
    return NULL;
}

void putback_hugetlb_folio(struct folio *folio) {}

#ifdef __cplusplus
}
#endif
