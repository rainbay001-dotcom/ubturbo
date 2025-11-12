/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 Tiering Memory Solution: tracking_dev
 */

#ifndef __TRACKING_PRIVATE_H__
#define __TRACKING_PRIVATE_H__

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/hrtimer.h>

int tracking_bus_init(void);
void tracking_bus_exit(void);

struct tracking_node_dev { /* cdev */
	struct list_head list;
	struct list_head dev_list; /* low level device */
	struct device *device;
	struct device cdev_device; /* cdev_device */
	struct cdev cdev;
	struct file *file;
	int target_node;
};

#endif /* __TRACKING_PRIVATE_H__ */