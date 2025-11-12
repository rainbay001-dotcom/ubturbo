/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_VMSTAT_H
#define _LINUX_VMSTAT_H

#include <linux/mmzone.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void mod_node_page_state(struct pglist_data *pgdat, enum node_stat_item item,
					long delta);

extern unsigned long sum_zone_node_page_state(int node,
				 enum zone_stat_item item);

static inline unsigned long zone_page_state(struct zone *zone,
					enum zone_stat_item item)
{
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_VMSTAT_H */
