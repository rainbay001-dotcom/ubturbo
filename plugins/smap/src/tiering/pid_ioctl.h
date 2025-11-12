/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap ioctl module
 */

#ifndef _SRC_TIERING_PID_IOCTL_H
#define _SRC_TIERING_PID_IOCTL_H

#include <linux/types.h>

#include "migrate_task.h"

#define SMAP_DEV "smap_dev"
#define SMAP_CLASS "smap_class"
#define SMAP_DEVICE "smap_device"

#define MAX_NR_MIGBACK 50

extern unsigned int smap_mode;
extern int nr_local_numa;

struct migrate_back_inner_msg {
	unsigned long long task_id;
	int count;
	struct migrate_back_inner_payload payload[MAX_NR_MIGBACK];
};

struct migrate_back_payload {
	int src_nid;
	int dest_nid;
	u64 memid;
};

enum task_id_mode {
	NORMAL_ID,
	DUP_ID,
	RETRY_ID,
};

struct migrate_back_msg {
	unsigned long long task_id;
	int count;
	struct migrate_back_payload payload[MAX_NR_MIGBACK];
};

#define SMAP_MAGIC 0xBA
#define SMAP_MIGRATE_BACK _IOW(SMAP_MAGIC, 0, struct migrate_back_msg)
#define BASE_MINOR 0
#define NR_MINOR 1

extern int smap_is_remote_addr_valid(int nid, u64 pa_start, u64 pa_end);
int smap_dev_init(void);
void smap_dev_exit(void);

#endif /* _SRC_TIERING_PID_IOCTL_H */
