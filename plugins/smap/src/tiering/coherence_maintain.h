/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef HAM_COHERENCE_MAINTAIN_H
#define HAM_COHERENCE_MAINTAIN_H
#include <linux/mm.h>
#include <linux/mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum coherence_maintain_action {
	CM_SUSPEND = 0,
	CM_RESUME = 1,
};

enum hisi_soc_cache_maint_type {
	HISI_CACHE_MAINT_CLEANSHARED,
	HISI_CACHE_MAINT_CLEANINVALID,
	HISI_CACHE_MAINT_MAKEINVALID,

	HISI_CACHE_MAINT_MAX
};

/* task_pgtable_set_cacheable only returns 0 currently */
int task_pgtable_within_mm_set_cacheable(struct mm_struct *mm,
					 unsigned long start,
					 unsigned long size, bool cacheable);

int task_pgtable_within_pid_set_cacheable(pid_t pid, unsigned long start,
					  unsigned long size, bool cacheable);

int kernel_pgtable_within_mm_set_valid(struct mm_struct *mm,
				       unsigned long start, unsigned long size,
				       bool valid);

int kernel_pgtable_within_pid_set_valid(pid_t pid, unsigned long start,
					unsigned long size, bool valid);

static inline int kernel_pgtable_within_pa_set_cacheable(unsigned long start,
							 unsigned long size,
							 bool cacheable)
{
	pr_info("HAM: Start modify kernel pgtable, size:0x%lx\n", size);
	return set_linear_mapping_nc(start >> PAGE_SHIFT,
				     (start + size) >> PAGE_SHIFT, !cacheable);
}

extern int hisi_soc_cache_maintain(phys_addr_t addr, size_t size,
				   enum hisi_soc_cache_maint_type mnt_type);

int flush_cache_by_pa(phys_addr_t addr, size_t size,
		      enum hisi_soc_cache_maint_type maint_type);

static inline int flush_cache_global(enum hisi_soc_cache_maint_type maint_type)
{
	return hisi_soc_cache_maintain(0, ~((size_t)0), maint_type);
}

#ifdef __cplusplus
}
#endif
#endif /* HAM_COHERENCE_MAINTAIN_H */