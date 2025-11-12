/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PGTABLE_H
#define _LINUX_PGTABLE_H

#include <asm/pgtable.h>
#include <asm/pgtable-types.h>
#include <stdbool.h>

#define pte_unmap(pte) ((void)(pte))
#define __pte_map(pmd, address) NULL
#define pte_cont(pte)   NULL
#define pte_page(pte)    (pte_pfn(pte))
#define pte_dirty(pte)    NULL
 
static inline int pmd_same(pmd_t pmd_a, pmd_t pmd_b)
{
	return 0;
}
 
static inline pmd_t pmdp_get_lockless(pmd_t *pmdp)
{
    pmd_t pmd = { .pmd = 1 };
    return pmd;
}
 
static inline pte_t __ptep_get(pte_t *ptep)
{
    pte_t pte = { .pte = 1 };
    return pte;
}
 
static inline pte_t pte_mkdirty(pte_t pte)
{
    pte_t ptep = { .pte = 1 };
    return ptep;
}
 
static inline pte_t pte_mkyoung(pte_t pte)
{
    pte_t ptep = { .pte = 1 };
    return ptep;
}
 
static inline bool pud_sect_supported(void)
{
    return true;
}
 
#endif /* _LINUX_PGTABLE_H */