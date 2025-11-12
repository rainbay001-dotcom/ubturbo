// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: 内核符号封装
 */

#include <linux/nodemask.h>
#include <linux/kprobes.h>

#include "smap_migrate_wrapper.h"

DEFINE_STATIC_KEY_FALSE(cpusets_pre_enable_key);
DEFINE_STATIC_KEY_FALSE(cpusets_enabled_key);

static inline unsigned long *get_pageblock_bitmap(const struct page *p,
						  unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	return section_to_usemap(__pfn_to_section(pfn));
#else
	return page_zone(p)->pageblock_flags;
#endif
}

static inline unsigned long pfn_to_bitidx(const struct page *p,
					  unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	pfn &= (PAGES_PER_SECTION - 1);
#else
	pfn = pfn -
	      round_down(page_zone(p)->zone_start_pfn, pageblock_nr_pages);
#endif
	return (pfn >> pageblock_order) * NR_PAGEBLOCK_BITS;
}

static __always_inline unsigned long
__get_pfnblock_flags_mask(const struct page *page, unsigned long pfn,
			  unsigned long mask)
{
	unsigned long bitidx, word_bitidx;
	unsigned long *bitmap = get_pageblock_bitmap(page, pfn);
	bitidx = pfn_to_bitidx(page, pfn);
	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG - 1);

	return (bitmap[word_bitidx] >> bitidx) & mask;
}

unsigned long get_pfnblock_flags_mask(const struct page *page,
				      unsigned long pfn, unsigned long mask)
{
	return __get_pfnblock_flags_mask(page, pfn, mask);
}
