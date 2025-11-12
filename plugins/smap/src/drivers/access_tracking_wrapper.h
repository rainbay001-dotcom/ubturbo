/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: common symbols for smap drivers
 */

#ifndef _ACCESS_TRACKING_WRAPPER_H
#define _ACCESS_TRACKING_WRAPPER_H

#include <asm/current.h>
#include <asm-generic/errno-base.h>

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/migrate.h>
#include <linux/hugetlb.h>
#include <linux/nodemask.h>
#include <linux/kernel.h>
#include <linux/pgtable.h>
#include <linux/sched.h>
#include <linux/lockdep.h>
#include <linux/page-flags.h>
#include <linux/mmdebug.h>
#include <linux/page_ref.h>
#include <linux/oom.h>
#include <linux/pageblock-flags.h>
#include <linux/bitops.h>
#include <linux/mmzone.h>
#include <linux/gfp.h>
#include <linux/numa.h>
#include <linux/cpuset.h>
#include <linux/spinlock_types.h>
#include <linux/mm_types.h>
#include <linux/rmap.h>
#include <linux/swapops.h>
#include <linux/kprobes.h>

pte_t *smap__pte_offset_map_lock(struct mm_struct *mm, pmd_t *pmd,
				 unsigned long addr, spinlock_t **ptlp);

static inline pte_t *smap_pte_offset_map_lock(struct mm_struct *mm, pmd_t *pmd,
					      unsigned long addr,
					      spinlock_t **ptlp)
{
	pte_t *pte = NULL;
	__cond_lock(*ptlp,
		    pte = smap__pte_offset_map_lock(mm, pmd, addr, ptlp));
	return pte;
}

pte_t smap_huge_ptep_get(pte_t *ptep);
#endif /* !_ACCESS_TRACKING_WRAPPER_H */