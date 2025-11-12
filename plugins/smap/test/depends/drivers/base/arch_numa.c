// SPDX-License-Identifier: GPL-2.0-only
/*
 * NUMA support, based on the x86 implementation.
 *
 * Copyright (C) 2015 Cavium Inc.
 * Author: Ganapatrao Kulkarni <gkulkarni@cavium.com>
 */

#include <linux/numa.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/mmzone.h>

#define pr_fmt(fmt) "NUMA: " fmt

struct pglist_data *node_data[MAX_NUMNODES] = { 0 };

int setup_node_data(int nid, u64 start_pfn, u64 end_pfn)
{
	if (nid >= MAX_NUMNODES)
		return -EINVAL;
	if (end_pfn < start_pfn)
		return -EINVAL;

	node_data[nid] = kzalloc(sizeof(pg_data_t), GFP_KERNEL);
	if (!node_data[nid])
		return -ENOMEM;
	node_data[nid]->node_id = nid;
	node_data[nid]->node_start_pfn = start_pfn;
	node_data[nid]->node_spanned_pages = end_pfn - start_pfn;
	return 0;
}

void teardown_node_data(int nid)
{
	if (!node_data[nid])
		return;
	kfree(node_data[nid]);
	node_data[nid] = NULL;
}
