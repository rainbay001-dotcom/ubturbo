/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * Description: SMAP3.0 Tiering Memory Solution: Self-check common configuration
 */

#ifndef DRIVERS_CHECK_H
#define DRIVERS_CHECK_H

#include <linux/mm.h>

#define ADDR_BH 0x7FFFFFFF
#define ADDR_AH 0x2080000000
#define TWO_MEGA_SHIFT 21
#define TWO_MEGA_SIZE (1UL << TWO_MEGA_SHIFT)
#define TWO_MEGA_MASK (~(TWO_MEGA_SIZE - 1))
#define FETCH_STEP 512
#define MAX_COUNTER 10000
#define MAX_ACTC_LEN 200000
#define MATCH_BIT 2
#define BIT_LENGTH 8
#define MILLION 1000000
#define GB_TO_4K_SHIFT (18)
#define HUGE_TO_4K_SHIFT (9)
#define PAGE_SIZE_2M 0x200000
#define PAGE_SIZE_4K 0x1000
#define PAGE_2M_SHIFT 21
#define PAGE_4K_SHIFT 12

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

#define SMAP_MAX_LOCAL_NUMNODES 4
#define SMAP_MAX_REMOTE_NUMNODES 18
#define SMAP_MAX_NUMNODES (SMAP_MAX_LOCAL_NUMNODES + SMAP_MAX_REMOTE_NUMNODES)

#define MAX_SIZE_PER_NUMA (1UL << 40)
#define MAX_NR_PAGE_PER_NUMA (MAX_SIZE_PER_NUMA >> PAGE_SHIFT)

enum node_level { L1, L2, NR_LEVEL };

/* calculate 2M count by addr range */
static inline u64 calc_2m_count(u64 range)
{
	return (range & ~TWO_MEGA_MASK) == 0 ?
		       (u64)(range >> TWO_MEGA_SHIFT) :
		       (u64)((range >> TWO_MEGA_SHIFT) + 1);
}

static inline u64 calc_4k_count(u64 range)
{
	return (range & ~PAGE_MASK) == 0 ? (u64)(range >> PAGE_SHIFT) :
					   (u64)((range >> PAGE_SHIFT) + 1);
}

#endif /* DRIVERS_CHECK_H */
