/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap access ioctl module
 */

#ifndef _SRC_ACCESS_IOCTL_H
#define _SRC_ACCESS_IOCTL_H

#include <linux/types.h>

#include "check.h"

#define ACCESS_DEV "smap_access_dev"
#define ACCESS_CLASS "smap_access_class"
#define ACCESS_DEVICE "smap_access_device"
#define BASE_MINOR 0
#define NR_MINOR 1
#define MAX_SCAN_DURATION_SEC 300

typedef enum {
	NO_SCAN = -1,
	HAM_SCAN,
	NORMAL_SCAN,
	STATISTIC_SCAN,
	MAX_SCAN_TYPE,
} scan_type;

struct access_add_pid_payload {
	pid_t pid;
	u32 numa_nodes;
	u32 scan_time;
	u32 duration;
	scan_type type;
	u32 ntimes;
};

struct access_add_pid_msg {
	int count;
	struct access_add_pid_payload *payload;
};

struct access_remove_pid_payload {
	pid_t pid;
};

struct access_remove_pid_msg {
	int count;
	struct access_remove_pid_payload *payload;
};

struct tracking_info_payload {
	pid_t pid;
	u32 length;
	u16 *data;
};

struct access_pid_freq_msg {
	pid_t pid;
	size_t len[SMAP_MAX_NUMNODES];
	u16 *freq[SMAP_MAX_NUMNODES];
};

#define SMAP_ACCESS_MAGIC 0xBB
#define SMAP_ACCESS_ADD_PID \
	_IOW(SMAP_ACCESS_MAGIC, 1, struct access_add_pid_msg)
#define SMAP_ACCESS_REMOVE_PID \
	_IOW(SMAP_ACCESS_MAGIC, 2, struct access_remove_pid_msg)
#define SMAP_ACCESS_REMOVE_ALL_PID _IOW(SMAP_ACCESS_MAGIC, 3, int)
#define SMAP_ACCESS_WALK_PAGEMAP _IOW(SMAP_ACCESS_MAGIC, 4, size_t)
#define SMAP_ACCESS_GET_TRACKING \
	_IOW(SMAP_ACCESS_MAGIC, 5, struct tracking_info_payload)
#define SMAP_ACCESS_READ_PID_FREQ \
	_IOW(SMAP_ACCESS_MAGIC, 6, struct access_pid_freq_msg)

void access_ioctl_exit(void);
int access_ioctl_init(void);

#endif /* _SRC_ACCESS_IOCTL_H */
