/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP : hist_dev
 */

#ifndef _SRC_HIST_TRACKING_H
#define _SRC_HIST_TRACKING_H

#include <linux/device.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>

extern struct list_head remote_ram_list;
extern rwlock_t rem_ram_list_lock;

extern bool ram_changed(void);
extern u64 get_node_page_cnt_iomem(int nid, int page_size);

struct node_info {
	u32 node;
	u64 pa;
	u64 size;
};

struct hist_tracking_dev {
	struct list_head list;
	struct device ldev;
	s8 node;

	void *tracking_dev;
	u16 *hist_actc_data;
	u64 page_count;
	u8 page_size_mode;
	u8 scan_mode;
	bool enable_on;

	u8 ba;
	struct node_info node_info;

	char workq_name[32];
	struct workqueue_struct *scanq;
	struct delayed_work scan_work;
	struct mutex mutex;
	struct completion work_done;
} __attribute__((aligned(8)));

#endif