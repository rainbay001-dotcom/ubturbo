/* SPDX-License-Identifier: GPL-2.0-only */
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 Tiering Memory Solution: tracking_dev
*/

#include <linux/device.h>

#define MEM_SIZE 0x10000000000

#define REG_BASE_ADDR_0 0x33030a0000
#define REG_BASE_ADDR_1 0x330a0a0000
#define REG_BASE_ADDR_2 0x73030a0000
#define REG_BASE_ADDR_3 0x730a0a0000

#define CC_MEM_BASE_ADDR_0 0x50000000000
#define CC_MEM_BASE_ADDR_1 0x60000000000
#define CC_MEM_BASE_ADDR_2 0xd0000000000
#define CC_MEM_BASE_ADDR_3 0xe0000000000

#define NC_MEM_BASE_ADDR_0 0x80000000000
#define NC_MEM_BASE_ADDR_1 0x90000000000
#define NC_MEM_BASE_ADDR_2 0x100000000000
#define NC_MEM_BASE_ADDR_3 0x110000000000

static u64 reg_base_addrs[] = {
	REG_BASE_ADDR_0,
	REG_BASE_ADDR_1,
	REG_BASE_ADDR_2,
	REG_BASE_ADDR_3
};

static u64 cc_mem_base_addrs[] = {
	CC_MEM_BASE_ADDR_0,
	CC_MEM_BASE_ADDR_1,
	CC_MEM_BASE_ADDR_2,
	CC_MEM_BASE_ADDR_3
};

static u64 nc_mem_base_addrs[] = {
	NC_MEM_BASE_ADDR_0,
	NC_MEM_BASE_ADDR_1,
	NC_MEM_BASE_ADDR_2,
	NC_MEM_BASE_ADDR_3
};

void device_initialize(struct device *dev)
{

}

int dev_set_name(struct device *dev, const char *name, ...)
{
	return 0;
}

int device_add(struct device *dev)
{
	return 0;
}

void device_del(struct device *dev)
{

}

struct device *get_device(struct device *dev)
{
	return NULL;
}

void put_device(struct device *dev)
{

}

struct device *device_create(struct class_stub *cls, struct device *parent,
			     dev_t devt, void *drvdata, const char *fmt, ...)
{
	return NULL;
}

void device_destroy(struct class_stub *cls, dev_t devt)
{
}

int device_create_file(struct device *device,
			const struct device_attribute *entry)
{
	return 0;
}

void device_remove_file(struct device *device,
			const struct device_attribute *entry)
{
}

int device_property_read_u64(struct device *dev, const char *propname, u64 *val)
{
	int i = dev->numa_node;
	if (strcmp(propname, "ba_reg_base_addr") == 0) {
		*val = reg_base_addrs[i];
	}
	if (strcmp(propname, "cc_mem_base_addr") == 0) {
		*val = cc_mem_base_addrs[i];
	}
	if (strcmp(propname, "cc_mem_size") == 0) {
		*val = MEM_SIZE;
	}
	if (strcmp(propname, "nc_mem_base_addr") == 0) {
		*val = nc_mem_base_addrs[i];
	}
	if (strcmp(propname, "nc_mem_size") == 0) {
		*val = MEM_SIZE;
	}
	return 0;
}
