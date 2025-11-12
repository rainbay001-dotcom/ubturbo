/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 acpi mem header
 */

#ifndef _ACPI_MEM_H
#define _ACPI_MEM_H

#include "common.h"

struct acpi_mem_segment {
	int node;
	int pxm;
	struct list_head segment;
	u64 start;
	u64 end;
};

struct mem_info {
	size_t len;
	int node_cached;
	struct list_head mem;
};

struct node_mem {
	bool online;
	u64 start;
	u64 end;
	int distance[SMAP_MAX_NUMNODES];
};

extern struct mem_info acpi_mem;
extern int nr_local_numa;

bool is_paddr_local(u64 pa);
int find_closest_node(int nid, int *cnid);
void print_acpi_mem(void);
int init_acpi_mem(void);
void reset_acpi_mem(void);

size_t calc_page_by_node(int node, int is_huge);

int get_node_actc_len(int len, u64 *node_actc_len);
int calc_paddr_acidx(u64 paddr, int *nid, u64 *index);

static inline bool is_numa_remote(int nid)
{
	return nid >= nr_local_numa && nid < SMAP_MAX_NUMNODES;
}

#endif /* _ACPI_MEM_H */
