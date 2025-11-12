// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: driver: tracking_bus
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/capability.h>

#include "tracking_private.h"
#include "bus.h"

#undef pr_fmt
#define pr_fmt(fmt) "tracking_bus: " fmt

static int match_always_count;
static DEFINE_IDA(tracking_instance_ida);
static DEFINE_MUTEX(tracking_bus_lock);
static int tracking_bus_probe(struct device *dev);
static void tracking_bus_remove(struct device *dev);
static int tracking_bus_match(struct device *dev, struct device_driver *drv);

static struct tracking_driver *to_tracking_drv(struct device_driver *drv)
{
	return container_of(drv, struct tracking_driver, drv);
}

static struct tracking_dev *to_tracking_dev(struct device *dev)
{
	return container_of(dev, struct tracking_dev, tdev);
}

static int tracking_bus_match(struct device *dev, struct device_driver *drv)
{
	struct tracking_driver *tracking_drv = to_tracking_drv(drv);

	if (tracking_drv->match_always) {
		return 1;
	}

	return 0;
}

static int tracking_bus_probe(struct device *dev)
{
	struct tracking_driver *trk_drv = to_tracking_drv(dev->driver);
	struct tracking_dev *trk_dev = to_tracking_dev(dev);
	int rc;

	rc = trk_drv->probe(trk_dev);

	return rc;
}
static void tracking_bus_remove(struct device *dev)
{
	struct tracking_driver *trk_drv = to_tracking_drv(dev->driver);
	struct tracking_dev *trk_dev = to_tracking_dev(dev);

	trk_drv->remove(trk_dev);
}

static struct bus_type tracking_bus_type = {
	.name = "tracking",
	.uevent = NULL,
	.match = tracking_bus_match,
	.probe = tracking_bus_probe,
	.remove = tracking_bus_remove,
};

int inner_tracking_driver_register(struct tracking_driver *tracking_drv,
				   struct module *module, const char *mod_name)
{
	struct device_driver *drv = &tracking_drv->drv;
	int rc = 0;

	drv->owner = module;
	drv->name = mod_name;
	drv->mod_name = mod_name;
	drv->bus = &tracking_bus_type;

	/* there can only be one default driver */
	mutex_lock(&tracking_bus_lock);
	match_always_count += tracking_drv->match_always;
	if (match_always_count > 1) {
		match_always_count--;
		WARN_ON(1);
		rc = -EINVAL;
	}

	mutex_unlock(&tracking_bus_lock);

	if (rc) {
		return rc;
	}

	return driver_register(drv);
}

void tracking_driver_unregister(struct tracking_driver *tracking_drv)
{
	struct device_driver *drv = &tracking_drv->drv;

	mutex_lock(&tracking_bus_lock);
	match_always_count -= tracking_drv->match_always;

	mutex_unlock(&tracking_bus_lock);
	driver_unregister(drv);
}

static void init_dev(struct tracking_dev *trk_dev, struct device *dev)
{
	device_initialize(dev);
	dev_set_name(dev, "tracking_dev%d", trk_dev->id);
	dev->bus = &tracking_bus_type;
	dev->parent = trk_dev->dev;
	dev_set_drvdata(dev, trk_dev);
}

struct tracking_dev *tracking_dev_add(struct device *ldev,
				      const struct tracking_operations *ops,
				      u8 target_node)
{
	int ret;
	struct tracking_dev *trk_dev;
	struct device *dev;

	trk_dev = kzalloc(sizeof(*trk_dev), GFP_KERNEL);
	if (!trk_dev)
		return NULL;

	trk_dev->dev = ldev;
	trk_dev->ops = ops;
	trk_dev->target_node = target_node;

	ret = ida_simple_get(&tracking_instance_ida, 0, 0, GFP_KERNEL);
	if (ret < 0) {
		goto out;
	}
	trk_dev->id = ret;

	dev = &trk_dev->tdev;
	init_dev(trk_dev, dev);

	ret = device_add(dev);
	if (ret) {
		pr_err("tracking device add failed!\n");
		goto ida_remove;
	}

	if (ldev) {
		ret = sysfs_create_link(&dev->kobj, &ldev->kobj,
					"low_level_dev");
	} else {
		pr_err("ldev is null\n");
		goto trk_dev_del;
	}

	if (ret) {
		pr_err("sysfs_create_link failed!\n");
		goto trk_dev_del;
	}

	pr_info("tracking dev add success, node = %d\n", target_node);
	return trk_dev;

trk_dev_del:
	device_del(&trk_dev->tdev);
ida_remove:
	ida_simple_remove(&tracking_instance_ida, trk_dev->id);
out:
	kfree(trk_dev);
	return NULL;
}

EXPORT_SYMBOL_GPL(tracking_dev_add);

void tracking_dev_remove(struct tracking_dev *trk_dev)
{
	sysfs_remove_link(&trk_dev->tdev.kobj, "low_level_dev");
	device_del(&trk_dev->tdev);
	ida_simple_remove(&tracking_instance_ida, trk_dev->id);
	kfree(trk_dev);
}

EXPORT_SYMBOL_GPL(tracking_dev_remove);

int tracking_bus_init(void)
{
	int ret;

	ret = bus_register(&tracking_bus_type);
	if (ret) {
		pr_err("tracking bus registration failed: %d\n", ret);
	}
	return ret;
}

void tracking_bus_exit(void)
{
	bus_unregister(&tracking_bus_type);
	ida_destroy(&tracking_instance_ida);
}