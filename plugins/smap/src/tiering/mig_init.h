/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP: SMAP MIGRATE
 */

#ifndef _TRACKING_MIG_H
#define _TRACKING_MIG_H

#include <linux/types.h>

#include "dump_info.h"
#include "smap_migrate_pages.h"
#include "smap_migrate_wrapper.h"

#define KB_TO_2M 11
#define KB_TO_4K 2
#define BASE_MINOR 0
#define NR_MINOR 1
#define SMAP_MIG_DEV "smap_mig_dev"
#define SMAP_MIG_CLASS "smap_mig_class"
#define SMAP_MIG_DEVICE "smap_mig_device"
#define SMAP_MIG_MAGIC 0xB9
#define SMAP_MIG_MIGRATE _IOW(SMAP_MIG_MAGIC, 0, struct migrate_msg)
#define SMAP_CHECK_PAGESIZE _IOW(SMAP_MIG_MAGIC, 1, uint32_t)
#define SMAP_MIG_MIGRATE_NUMA _IOW(SMAP_MIG_MAGIC, 2, struct migrate_numa_msg)
#define SMAP_MIG_PID_REMOTE_NUMA \
	_IOW(SMAP_MIG_MAGIC, 3, struct migrate_pid_remote_numa_msg)

typedef struct {
	u64 pme;
} pagemap_entry_t;

struct remote_migrate_info {
	pid_t pid;
	u64 page_cnt;
	int remote_nid;
	unsigned int mig_cnt;
	u64 folios_len;
	struct folio **folios;
};

struct pagemapread {
	int pos, len; /* units: PM_ENTRY_BYTES, not bytes */
	migrate_type mig_type;
	struct remote_migrate_info mig_info;
	struct access_pid *ap;
};

static inline int get_max_pid_cnt(void)
{
	return smap_pgsize == HUGE_PAGE ? MAX_2M_PROCESSES_CNT :
					  MAX_4K_PROCESSES_CNT;
}

extern void walk_pid_pagemap(struct pagemapread *pm);
extern int convert_pos_to_paddr_sorted(pid_t pid, int nid, u64 len, u64 *addr);
extern int smap_is_remote_addr_valid(int nid, u64 pa_start, u64 pa_end);

extern u64 get_node_page_cnt_iomem(int nid, int page_size);
int init_migrate(void);
void exit_migrate(void);

#endif /* _TRACKING_MIG_H */
