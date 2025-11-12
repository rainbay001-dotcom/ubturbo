// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 Tiering Memory Solution: tracking_dev
*/

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <linux/dev_printk.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/device/bus.h>
#include <linux/device/driver.h>
#include <linux/bug.h>
#include <linux/device/class.h>
#include <linux/byteorder/generic.h>
#include <linux/slab.h>
#ifdef __cplusplus
extern "C" {
#endif

#define KBUILD_MODNAME "tracking_bus"

struct device {
    struct kobject kobj;
    struct device *parent;
	int		numa_node;	/* NUMA node this device is close to */
    struct  device_driver *driver;
    struct  bus_type *bus;
    void *driver_data;
    dev_t			devt;	/* dev_t, creates the sysfs "dev" */
    struct class_stub *class_stub;
    u32 scan_time;
};

void device_initialize(struct device *dev);

int dev_set_name(struct device *dev, const char *name, ...);

int device_add(struct device *dev);

void device_del(struct device *dev);

struct device_attribute {
    ssize_t (*store)(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
    ssize_t (*show)(struct device *dev, struct device_attribute *attr, char *buf);
    /* device_attribute attr */
    struct attribute attr;
};

static inline void *dev_get_drvdata(const struct device *dev)
{
    return dev->driver_data;
}

static inline void *devm_ioremap_resource(struct device *dev, const struct resource *res)
{
    return NULL;
}

static inline void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
    return kzalloc(size, gfp);
}

static inline void devm_kfree(struct device *dev, void *ptr)
{
    kfree(ptr);
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
    dev->driver_data = data;
}

int device_property_read_u64(struct device *dev, const char *propname, u64 *val);
struct device *get_device(struct device *dev);
void put_device(struct device *dev);

struct device *device_create(struct class_stub *cls, struct device *parent,
    dev_t devt, void *drvdata, const char *fmt, ...);
void device_destroy(struct class_stub *cls, dev_t devt);

#define DEVICE_ATTR(_name, _mode, _show, _store) \
	struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)

#ifdef __cplusplus
}
#endif

#endif /* _DEVICE_H_ */
