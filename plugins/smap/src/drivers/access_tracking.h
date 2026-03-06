/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: tracking_access模块
 */

#ifndef _SRC_ACCESS_TRACKING_H
#define _SRC_ACCESS_TRACKING_H

#include <linux/device.h>
#include <linux/types.h>
#include <linux/mutex.h>

#include "accessed_bit.h"
#include "access_pid.h"
#include "drv_common.h"
#include "bus.h"
#include "hist_tracking.h"

extern unsigned int smap_scene;
extern u32 g_pagesize_huge;

enum access_page_mode {
	PAGE_MODE_4K = 0,
	PAGE_MODE_2M = 9, // 考虑上层提供的其他常见页面粒度，这里对此做预留
};

enum smap_scene_args {
	NORMAL_SCENE,
	UB_QEMU_SCENE,
	UB_QEMU_SCENE_ADVANCED,
	NR_SCENE_ARGS,
};

enum hist_status {
	DISABLE_HIST,
	ENABLE_HIST,
	NR_STATUS_ARGS,
};

#define AB_ACTC_ELEM_SIZE 16
#define WORKQ_NAME_SIZE 32
#define WORKQ_NAME_MAX_LEN (WORKQ_NAME_SIZE - 1)
#define WQ_MAX_THREADS 8

extern struct list_head access_dev;
extern u8 access_page_size;

static inline bool is_access_hugepage(void)
{
	return access_page_size == PAGE_MODE_2M;
}

static inline struct access_tracking_dev *get_access_tracking_dev(int node_id)
{
	struct access_tracking_dev *adev = NULL;
	list_for_each_entry(adev, &access_dev, list) {
		if (adev->node == node_id) {
			break;
		}
	}
	return adev;
}

static inline struct access_tracking_dev *get_first_access_dev(void)
{
	return list_first_entry(&access_dev, struct access_tracking_dev, list);
}

static inline int get_page_size(struct access_tracking_dev *adev)
{
	return adev->page_size_mode == PAGE_MODE_2M ? g_pagesize_huge :
						      PAGE_SIZE;
}

void cancel_ap_scan_work(struct access_pid *ap);
bool is_access_hugepage(void);
void submit_one_work(struct access_pid *ap);
ktime_t calc_time_us(ktime_t start_time);
#endif /* _SRC_ACCESS_TRACKING_H */
