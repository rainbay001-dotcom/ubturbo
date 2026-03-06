/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: SMAP3.0 Tiering Memory Solution: Self-check common configuration
 */

#ifndef DRIVERS_CHECK_H
#define DRIVERS_CHECK_H

#include <linux/mm.h>
#define TWO_MEGA_SHIFT 21
#define TWO_MEGA_SIZE (1UL << TWO_MEGA_SHIFT)
#define TWO_MEGA_MASK (~(TWO_MEGA_SIZE - 1))
#define GB_TO_4K_SHIFT (18)
#define HUGE_TO_4K_SHIFT (__builtin_ctz(g_pagesize_huge) - PAGE_SHIFT)

#define PAGE_SIZE_4K (1UL << 12)
#define PAGE_SIZE_64K (1UL << 16)
#define PAGE_SIZE_2M (1UL << 21)


#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

#define SMAP_MAX_LOCAL_NUMNODES 4
#define SMAP_MAX_REMOTE_NUMNODES 18
#define SMAP_MAX_NUMNODES (SMAP_MAX_LOCAL_NUMNODES + SMAP_MAX_REMOTE_NUMNODES)

#define MAX_SIZE_PER_NUMA (1UL << 40)
#define MAX_NR_PAGE_PER_NUMA (MAX_SIZE_PER_NUMA >> PAGE_SHIFT)

enum node_level { L1, L2, NR_LEVEL };

extern u32 g_pagesize_huge;

static inline u64 calc_huge_count(u64 range)
{
	return (range & ~TWO_MEGA_MASK) == 0 ?
		       (u64)(range >> TWO_MEGA_SHIFT) :
		       (u64)((range >> TWO_MEGA_SHIFT) + 1);
}

static inline u64 calc_normal_count(u64 range)
{
	return (range & ~PAGE_MASK) == 0 ? (u64)(range >> PAGE_SHIFT) :
					   (u64)((range >> PAGE_SHIFT) + 1);
}

#endif /* DRIVERS_CHECK_H */
