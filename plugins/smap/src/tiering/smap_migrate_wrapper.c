// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: wrapper
 */

#include <linux/nodemask.h>
#include <linux/kprobes.h>

#include "smap_migrate_wrapper.h"

DEFINE_STATIC_KEY_FALSE(cpusets_pre_enable_key);
DEFINE_STATIC_KEY_FALSE(cpusets_enabled_key);

unsigned long (*fp_kallsyms_lookup_name)(const char *) = NULL;
int (*fp_migrate_pages)(struct list_head *from, new_folio_t get_new_folio,
	free_folio_t put_new_folio, unsigned long private,
	enum migrate_mode mode, int reason, unsigned int *ret_succeeded) = NULL;
void (*fp_putback_movable_pages)(struct list_head *l) = NULL;
bool (*fp_isolate_folio_to_list)(struct folio *folio, struct list_head *list) = NULL;
unsigned long (*fp_reclaim_pages)(struct list_head *folio_list) = NULL;

static inline unsigned long *get_pageblock_bitmap(const struct page *p,
						  unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	return section_to_usemap(__pfn_to_section(pfn));
#else
	return page_zone(p)->pageblock_flags;
#endif
}

void lookup_kallsyms_lookup_name(void)
{
	struct kprobe kp;
	memset(&kp, 0, sizeof(struct kprobe));
	kp.symbol_name = "kallsyms_lookup_name";
	if (register_kprobe(&kp) < 0) {
		return;
	}
	fp_kallsyms_lookup_name = (unsigned long (*)(const char *))kp.addr;
	unregister_kprobe(&kp);
}

int smap_process_symbols(void)
{
	lookup_kallsyms_lookup_name();
	if (!fp_kallsyms_lookup_name)
		return -EFAULT;

	fp_migrate_pages = (int (*)(struct list_head *from, new_folio_t get_new_folio,
		free_folio_t put_new_folio, unsigned long private,
		enum migrate_mode mode, int reason, unsigned int *ret_succeeded))
		fp_kallsyms_lookup_name("migrate_pages");
	fp_putback_movable_pages = (void (*)(struct list_head *l))
		fp_kallsyms_lookup_name("putback_movable_pages");
	fp_isolate_folio_to_list = (bool (*)(struct folio *folio, struct list_head *list))
		fp_kallsyms_lookup_name("isolate_folio_to_list");
	if (!(fp_migrate_pages && fp_putback_movable_pages && fp_isolate_folio_to_list))
		return -EFAULT;

	/* reclaim_pages: force page reclaim to swap (Linux 5.16+) */
	fp_reclaim_pages = (unsigned long (*)(struct list_head *))
		fp_kallsyms_lookup_name("reclaim_pages");
	if (!fp_reclaim_pages)
		pr_warn("smap: reclaim_pages not found, swap-out disabled\n");

	return 0;
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
