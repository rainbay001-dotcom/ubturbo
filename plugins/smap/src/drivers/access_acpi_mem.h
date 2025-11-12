/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP acpi mem header
 */

#ifndef _ACPI_MEM_H
#define _ACPI_MEM_H

#include "access_iomem.h"

struct acpi_mem_segment {
	int node;
	int pxm;
	struct list_head segment;
	u64 start;
	u64 end;
};

struct mem_info {
	size_t len;
	struct list_head mem;
};
extern struct mem_info acpi_mem;
extern int nr_local_numa;

bool is_paddr_local(u64 pa);
int init_acpi_mem(void);
void reset_acpi_mem(void);
u64 get_node_actc_len(int node_id, int page_size);
int calc_paddr_acidx_acpi(u64 paddr, int *nid, u64 *index, int page_size);
int calc_acidx_paddr_acpi(int nid, u64 acidx, u64 *paddr, int page_size);

static inline int convert_nid_to_pos(int nid)
{
	return nid >= nr_local_numa ?
		       nid - nr_local_numa + SMAP_MAX_LOCAL_NUMNODES :
		       nid;
}

/* 将numa_node中的偏移量转换为远端numa */
static inline int convert_pos_to_nid(unsigned long pos)
{
	return pos >= SMAP_MAX_LOCAL_NUMNODES ? (pos - SMAP_MAX_LOCAL_NUMNODES +
						 (unsigned long)nr_local_numa) :
						pos;
}

#endif /* _ACPI_MEM_H */
