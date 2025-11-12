/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_PGTABLE_H
#define __ASM_PGTABLE_H

#include <asm/pgtable-hwdef.h>
#include <asm/pgtable-prot.h>
#include <asm-generic/rwonce.h>
#include <asm/pgtable-types.h>
#include <stdbool.h>
#include <linux/version.h>

#define HPAGE_SHIFT		PMD_SHIFT
#define PAGE_SHIFT		12
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)
#define _AT(T, X)	((T)(X))
#define PTE_ADDR_LOW		(((_AT(pteval_t, 1) << (48 - PAGE_SHIFT)) - 1) << PAGE_SHIFT)
#define PTE_ADDR_MASK		PTE_ADDR_LOW

#define __pte_to_phys(pte)	(pte_val(pte) & PTE_ADDR_MASK)
#define __pmd_to_phys(pmd)	__pte_to_phys(pmd_pte(pmd))
#define pte_pfn(pte)		(__pte_to_phys(pte) >> PAGE_SHIFT)
#define pte_young(pte)		0

#define pgd_offset(mm, address)		mm
#define p4d_none(p4d)		0
#define p4d_bad(p4d)		0
#define pud_offset(mm, address)		0
#define pud_none(pud)		0
#define pud_bad(pud)		0
#define pmd_offset(mm, address)		0
#define pmd_none(pmd)		0
#define pmd_bad(pmd)		0
#define pte_unmap(pte) ((void)(pte))
#define pte_none(pte)		(!pte_val(pte))

static inline int pgd_none(pgd_t pgd)		{ return 0; }
static inline int pgd_bad(pgd_t pgd)		{ return 0; }

#define __pte_to_phys(pte)	(pte_val(pte) & PTE_ADDR_MASK)
#define __pmd_to_phys(pmd)	__pte_to_phys(pmd_pte(pmd))
#define __pud_to_phys(pud)	__pte_to_phys(pud_pte(pud))

#define pte_present(p)  ((p).pte != 0)
#define pte_devmap(p)   ((p).pte != 0)

#define pte_valid(p)    ((p).pte != 0)

#define pmd_valid(pmd)		pte_valid(pmd_pte(pmd))
#define pmd_devmap(pmd)		pte_devmap(pmd_pte(pmd))

#define pmd_present_invalid(pmd)     (!!(pmd_val(pmd) & PMD_PRESENT_INVALID))

#define pmd_ERROR(e)	\
	printf("%s:%d: bad pmd %016llx.\n", __FILE__, __LINE__, pmd_val(e))

#define pgprot_tagged(x) x
#define pgprot_writecombine(x) x

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
static inline int __ptep_test_and_clear_young(void *, int, pte_t *ptep)
#else
static inline int __ptep_test_and_clear_young(pte_t *ptep)
#endif
{
	return 0;
}

static inline int set_linear_mapping_invalid(unsigned long start_pfn, unsigned long end_pfn, bool invalid)
{
    return 0;
}

static inline int set_linear_mapping_nc(unsigned long start_pfn, unsigned long end_pfn, bool set_nc)
{
    return 0;
}

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
    return pte;
}

static inline pte_t pmd_pte(pmd_t pmd)
{
    return __pte(pmd_val(pmd));
}

static inline int pmd_present(pmd_t pmd)
{
    return pte_present(pmd_pte(pmd)) || pmd_present_invalid(pmd);
}

static inline void pmd_clear(pmd_t *pmdp)
{
    pmdp->pmd = 0;
}

static inline int pmd_trans_huge(pmd_t pmd)
{
    return pmd.pmd != 0;
}

static inline p4d_t *p4d_offset(pgd_t *pgd, unsigned long address)
{
	return (p4d_t *)pgd;
}

static inline pte_t pud_pte(pud_t pud)
{
	return __pte(pud_val(pud));
}

static inline pgprot_t pte_pgprot(pte_t pte)
{
    return __pgprot(pte_val(pte));
}

static inline void __set_pte(pte_t *ptep, pte_t pte)
{
}

#ifdef __cplusplus
extern "C" {
#endif

pte_t ptep_get(pte_t *ptep);

#ifdef __cplusplus
}
#endif

#endif /* __ASM_PGTABLE_H */
