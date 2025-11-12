/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/mmzone.h>

void mod_node_page_state(struct pglist_data *pgdat, enum node_stat_item item,
					long delta)
{}

unsigned long sum_zone_node_page_state(int node,
				 enum zone_stat_item item)
{
	return 0;
}

void inc_node_page_state(struct page *page, enum node_stat_item item)
{}
