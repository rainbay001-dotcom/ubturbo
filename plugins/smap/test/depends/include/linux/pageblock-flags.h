/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef PAGEBLOCK_FLAGS_H
#define PAGEBLOCK_FLAGS_H

#include <asm/pgtable.h>

#define PB_migratetype_bits 3

enum pageblock_bits {
	PB_migrate = 0,
	PB_migrate_end = PB_migrate + PB_migratetype_bits - 1,
	PB_migrate_skip,
	NR_PAGEBLOCK_BITS
};

#define pageblock_order		HUGETLB_PAGE_ORDER

#endif
