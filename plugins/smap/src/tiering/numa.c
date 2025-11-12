// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP numa module
 */

#include <linux/mm.h>
#include <linux/hugetlb.h>

#include "smap_msg.h"

unsigned long get_node_nr_free_pages(int nid)
{
	pg_data_t *pgdat;
	struct zone *zones;
	int i;
	unsigned long count = 0;

	pgdat = NODE_DATA(nid);
	if (!pgdat) {
		return 0;
	}

	zones = pgdat->node_zones;
	for (i = 0; i < MAX_NR_ZONES; i++)
		count += zone_page_state(zones + i, NR_FREE_PAGES);

	return count;
}
