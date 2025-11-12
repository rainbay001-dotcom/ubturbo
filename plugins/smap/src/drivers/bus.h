/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: driver: tracking_bus
 */

#ifndef __TRACKING_BUS_H__
#define __TRACKING_BUS_H__

#include <linux/device.h>

#define MAX_NODE_NUM 32

struct tracking_operations {
	void (*tracking_enable)(struct device *ldev);
	void (*tracking_disable)(struct device *ldev);
	int (*tracking_mode_set)(struct device *ldev, u8 trk_mode);
	int (*tracking_set_page_size)(struct device *ldev, u8 page_size);
	int (*tracking_reinit_actc_buffer)(struct device *ldev);
	int (*tracking_set_sample_rate)(struct device *ldev, u32 sample_rate);
	int (*tracking_reinit_node)(struct device *ldev);
	int (*tracking_read)(struct device *ldev, void *buffer, u32 length);

#ifdef ENV_USER
	int (*tracking_ram_change)(struct device *ldev, void __user *argp);
	int (*tracking_acpi_len_get)(struct device *ldev, void __user *argp);
	int (*tracking_iomem_len_get)(struct device *ldev, void __user *argp);
	int (*tracking_acpi_mem_get)(struct device *ldev, void __user *argp);
	int (*tracking_iomem_get)(struct device *ldev, void __user *argp);
#endif
};

struct tracking_dev {
	struct list_head list;
	struct device *dev; /* low level device eg.pcie i2c usb */
	struct device tdev; /* device added to tracking bus */
	int target_node;
	int id;
	const struct tracking_operations *ops;
	void *trk_node_device; /* pointer to node_dev struct */
};

enum MODE_TYPE {
	ACTC_MODE = 0,
	CPI_MODE_SUM = 2,
	ACCESS_MODE_AND = 4,
	ACCESS_MODE_SUM = 5,
	ACCESS_MODE_OR = 6,
	MEBS_MODE = 7,
	MEBS_MODE_4B = 8,
	MEBS_MODE_6B = 9,
	PLDA_HDT_MODE = 10,
	PLDA_HDT_DECAY_MODE = 11,
	MODE_MAX
};

enum MAP_MODE_TYPE {
	ADDR_MAP_MODE = 0,
	INTERLEAVED_MAP_MODE = 1,
	MAP_MODE_MAX
};

struct tracking_driver {
	struct device_driver drv;
	int match_always;
	int (*probe)(struct tracking_dev *dev);
	void (*remove)(struct tracking_dev *dev);
};

int inner_tracking_driver_register(struct tracking_driver *tracking_drv,
				   struct module *module, const char *mod_name);
#define tracking_driver_register(driver) \
	inner_tracking_driver_register(driver, THIS_MODULE, KBUILD_MODNAME)
void tracking_driver_unregister(struct tracking_driver *tracking_drv);
struct tracking_dev *tracking_dev_add(struct device *ldev,
				      const struct tracking_operations *ops,
				      u8 target_node);
void tracking_dev_remove(struct tracking_dev *trk_dev);
int tracking_core_reinit_actc_buffer(int nid);

#endif /* __TRACKING_BUS_H__ */