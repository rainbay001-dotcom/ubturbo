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

extern unsigned int smap_scene;

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

#define AB_ACTC_ELEM_SIZE 16
#define WORKQ_NAME_SIZE 32
#define WORKQ_NAME_MAX_LEN (WORKQ_NAME_SIZE - 1)
#define WQ_MAX_THREADS 8

extern struct list_head access_dev;

struct access_tracking_dev {
	struct list_head list;
	struct device ldev;
	s8 node;

	void *tracking_dev;
	actc_t *access_bit_actc_data;
	u64 page_count;
	u8 page_size_mode;
	u8 mode;

	char workq_name[32];
	struct workqueue_struct *scanq;
	struct rw_semaphore buffer_lock;
	struct completion work_done;
} __attribute__((aligned(8)));

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

bool is_access_hugepage(void);
void submit_one_work(struct access_pid *ap);
ktime_t calc_time_us(ktime_t start_time);
#endif /* _SRC_ACCESS_TRACKING_H */
