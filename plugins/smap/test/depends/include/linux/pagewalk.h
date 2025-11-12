/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PAGEWALK_H
#define _LINUX_PAGEWALK_H

#include <asm/pgtable.h>
#include <linux/mm.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mm_walk;

#define CONT_PMD_MASK 0
#define PUD_TABLE_BIT 0
#define PMD_TABLE_BIT 0

static int p4d_present(p4d_t p4d)
{
    return 0;
}

static int pgd_present(pgd_t pgdp)
{
    return 0;
}

static int pud_present(pud_t pud)
{
    return 0;
}

static inline pte_t *pte_offset_kernel(pmd_t *pmd, unsigned long address)
{
    return 0;
}

enum page_walk_action {
	ACTION_SUBTREE = 0,
	ACTION_CONTINUE = 1,
	ACTION_AGAIN = 2
};

// Define the structure for memory memory_walk operations, containing various callback function pointers
struct mm_walk_ops {
    // Callback function to handle page global directory (PGD) entries
    int (*pgd_entry)(pgd_t *page_gd, unsigned long address, unsigned long next_entry, struct mm_walk *memory_walk);
    char reserved1;
    // Callback function to handle page upper directory (P4D) entries
    int (*p4d_entry)(p4d_t *page_4d, unsigned long address, unsigned long next_entry, struct mm_walk *memory_walk);
    char reserved2;
    // Callback function to handle page upper directory (PUD) entries
    int (*pud_entry)(pud_t *page_ud, unsigned long address, unsigned long next_entry, struct mm_walk *memory_walk);
    char reserved3;
    // Callback function to handle page middle directory (PMD) entries
    int (*pmd_entry)(pmd_t *page_md, unsigned long address, unsigned long next_entry, struct mm_walk *memory_walk);
    char reserved4;
    // Callback function to handle page table entries (PTE)
    int (*pte_entry)(pte_t *page_te, unsigned long address, unsigned long next_entry, struct mm_walk *memory_walk);
    char reserved5;
    // Callback function to handle holes in the page table
    int (*pte_hole)(unsigned long address, unsigned long next_entry, int depth, struct mm_walk *memory_walk);
    char reserved6;
    // Callback function to handle huge page table entries
    int (*hugetlb_entry)(pte_t *page_te, unsigned long hmask, unsigned long address, unsigned long next_entry,
                 struct mm_walk *memory_walk);
    char reserved7;
    // Callback function to test the memory_walk operation
    int (*test_walk)(unsigned long address, unsigned long next_entry, struct mm_walk *memory_walk);
    char reserved8;
    // Callback function to be called before traversing a VMA
    int (*pre_vma)(unsigned long start, unsigned long end, struct mm_walk *memory_walk);
    char reserved9;
    // Callback function to be called after traversing a VMA
    void (*post_vma)(struct mm_walk *memory_walk);
};

// Define the structure for memory memory_walk context
struct mm_walk {
    // Private data to be passed to callback functions
    void *private_data;

    // Flag indicating whether there is no VMA
    bool no_vma;

    // Action type for the memory_walk operation
    enum page_walk_action action;

    // Pointer to the current virtual memory area (VMA)
    struct vm_area_struct *vma;

    // Pointer to the page global directory (PGD)
    pgd_t *pgd;

    // Pointer to the memory management structure (mm_struct)
    struct mm_struct *mm;

    // Pointer to the memory memory_walk operations structure
    const struct mm_walk_ops *ops;
};

int walk_page_range(struct mm_struct *mm, unsigned long start, unsigned long end, const struct mm_walk_ops *ops,
                    void *pri);

#ifdef __cplusplus
}
#endif

#endif
