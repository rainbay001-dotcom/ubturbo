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

extern u32 g_pagesize_huge;

struct access_tracking_dev {
	struct list_head list;
	struct device ldev;
	s8 node;

	void *tracking_dev;
	u16 *access_bit_actc_data;
	u64 page_count;
	u8 page_size_mode;
	bool enable_on;
	bool is_hist;

	u8 ba;

	char workq_name[32];
	struct workqueue_struct *scanq;
	struct delayed_work scan_work;
	struct rw_semaphore buffer_lock;
	struct completion work_done;
} __attribute__((aligned(8)));

void access_tracking_dev_release(struct device *dev);
int hist_module_init(void);

#endif