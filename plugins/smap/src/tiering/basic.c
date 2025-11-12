// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "basic.h"
#include <linux/mm.h>

#undef pr_fmt
#define pr_fmt(fmt) "HAM: " fmt

static int kernel_num_contig_ptes(unsigned long size, size_t *pgsize)
{
	int ptes = 0;

	*pgsize = size;

	switch (size) {
#ifndef __PAGETABLE_PMD_FOLDED
	case PUD_SIZE:
		if (pud_sect_supported()) {
			ptes = 1;
		}
		break;
#endif
	case PMD_SIZE:
		ptes = 1;
		break;
	case CONT_PMD_SIZE:
		*pgsize = PMD_SIZE;
		ptes = CONT_PMDS;
		break;
	case CONT_PTE_SIZE:
		*pgsize = PAGE_SIZE;
		ptes = CONT_PTES;
		break;
	}

	return ptes;
}

pte_t kernel_huge_ptep_get(pte_t *ptep)
{
	int ncontig, i;
	size_t pgsize;
	pte_t orig_pte = __ptep_get(ptep);
	if (!pte_present(orig_pte) || !pte_cont(orig_pte))
		return orig_pte;

	ncontig =
		kernel_num_contig_ptes(page_size(pte_page(orig_pte)), &pgsize);
	for (i = 0; i < ncontig; i++, ptep++) {
		pte_t pte = __ptep_get(ptep);
		if (pte_dirty(pte))
			orig_pte = pte_mkdirty(orig_pte);

		if (pte_young(pte))
			orig_pte = pte_mkyoung(orig_pte);
	}
	return orig_pte;
}

/*
 * Find a task by it's virtual pid and get the mm_struct, and the mmput()
 * needs to be called subsequently.
 */
struct mm_struct *find_get_mm_by_vpid(pid_t pid)
{
	struct task_struct *task;
	struct mm_struct *mm = NULL;

	/* Get process information */
	task = find_get_task_by_vpid(pid);
	if (!task) {
		pr_err("Failed to get task_struct of PID:%d\n", pid);
		return NULL;
	}

	/* Get mm_struct */
	mm = get_task_mm(task);
	if (!mm) {
		pr_err("Failed to get mm_struct of PID:%d\n", pid);
	}
	put_task_struct(task);
	return mm;
}