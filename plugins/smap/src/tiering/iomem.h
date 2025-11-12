/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: smap5.0 remote iomem module
 */

#ifndef _SRC_TIERING_IOMEM_H
#define _SRC_TIERING_IOMEM_H

#include <linux/list.h>

#include "common.h"

#define REMOTE_NUMA_ID 4
#define REMOTE_PA_START 0x40000000000
#define REMOTE_PA_END 0x47fffffffff

#define MAX_MEMID_STRLEN 32

struct ram_segment {
	struct list_head node;
	int numa_node;
	u64 start;
	u64 end;
};

extern struct list_head remote_ram_list;
extern int nr_local_numa;

struct memid_range {
	u64 memid;
	u64 start;
	u64 end;
	u64 seq;
	struct list_head node;
};

int iterate_obmm_dev(void);
void release_remote_ram(void);
int refresh_remote_ram(void);
int calc_acidx_paddr_iomem(u64 index, int nid, u64 *paddr);
int find_range_by_memid(u64 memid, u64 *start, u64 *end);

#endif /* _SRC_TIERING_IOMEM_H */
