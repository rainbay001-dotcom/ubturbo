// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: driver: tracking_core
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>

#include "tracking_ioctl.h"
#include "tracking_private.h"
#include "bus.h"

#undef pr_fmt
#define pr_fmt(fmt) "tracking_core: " fmt
#define DEFAULT_NR_SEG 1

struct tracking_core_ctrl {
	unsigned long node_bitmap;
	struct list_head node_cdev;
};

static struct class *tracking_class;
static struct tracking_core_ctrl *trk_core_ctrl;
static dev_t tracking_chr_devt;

#define NUM 10
void activate_stack_protect(void)
{
	int i;
	int test_vec[NUM];
	test_vec[NUM - 1] = NUM;
	for (i = 0; i < NUM; i++) {
		pr_info("%d", test_vec[i]);
	}
}

static int node_cdev_open(struct inode *inode, struct file *file)
{
	struct tracking_node_dev *node_dev =
		container_of(inode->i_cdev, struct tracking_node_dev, cdev);

	file->private_data = node_dev;
	return 0;
}

static int node_cdev_release(struct inode *inode, struct file *file)
{
	return 0;
}

void node_tracking_enable(struct tracking_node_dev *node_dev)
{
	struct tracking_dev *trk_dev;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_enable)
			trk_dev->ops->tracking_enable(trk_dev->dev);
	}
}

void node_tracking_disable(struct tracking_node_dev *node_dev)
{
	struct tracking_dev *trk_dev;

	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_disable)
			trk_dev->ops->tracking_disable(trk_dev->dev);
	}
}

int node_tracking_set_page_size(struct tracking_node_dev *node_dev,
				u8 page_size)
{
	struct tracking_dev *trk_dev;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_set_page_size)
			if (trk_dev->ops->tracking_set_page_size(trk_dev->dev,
								 page_size))
				return -EINVAL;
	}
	return 0;
}

int node_tracking_reinit_node(struct tracking_node_dev *node_dev)
{
	struct tracking_dev *trk_dev;
	int ret = 0;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_reinit_node)
			ret = trk_dev->ops->tracking_reinit_node(trk_dev->dev);
		if (ret)
			return ret;
	}
	return 0;
}

int node_tracking_set_trk_mode(struct tracking_node_dev *node_dev, u8 mode)
{
	struct tracking_dev *trk_dev;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_mode_set)
			if (trk_dev->ops->tracking_mode_set(trk_dev->dev, mode))
				return -EINVAL;
	}
	return 0;
}

#ifdef ENV_USER
static long handle_tracking_cmd(struct tracking_node_dev *node_dev,
				unsigned long arg)
{
	if (arg == TRACKING_DISABLED) {
		node_tracking_disable(node_dev);
	} else {
		node_tracking_enable(node_dev);
	}
	return 0;
}

static long handle_mode_set_cmd(struct tracking_node_dev *node_dev,
				unsigned long arg)
{
	if (arg >= MODE_MAX) {
		pr_err("not support trk mode %lu!\n", arg);
		return -EINVAL;
	}
	if (node_tracking_set_trk_mode(node_dev, (u8)arg)) {
		pr_err("the device does not support the mode %lu!\n", arg);
		return -EINVAL;
	}
	return 0;
}

static long handle_page_size_set_cmd(struct tracking_node_dev *node_dev,
				     unsigned long arg)
{
	if (node_tracking_set_page_size(node_dev, (u8)arg)) {
		pr_err("the device not support the page size %lu!\n", arg);
		return -EINVAL;
	}
	return 0;
}

static long handle_reinit_node_cmd(struct tracking_node_dev *node_dev)
{
	int ret = node_tracking_reinit_node(node_dev);
	if (ret) {
		pr_err("reinit node failed: %d\n", ret);
		return ret;
	}
	return 0;
}

int node_ram_change_cmd(struct tracking_node_dev *node_dev, void __user *argp)
{
	struct tracking_dev *trk_dev;
	int ret = 0;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_ram_change)
			ret = trk_dev->ops->tracking_ram_change(trk_dev->dev,
								argp);
		if (ret)
			return ret;
	}
	return 0;
}

static long handle_ram_change_cmd(struct tracking_node_dev *node_dev,
				  void __user *argp)
{
	int ret = node_ram_change_cmd(node_dev, argp);
	if (ret) {
		pr_err("node ram_change get failed: %d\n", ret);
		return ret;
	}
	return 0;
}

int node_acpi_len_cmd(struct tracking_node_dev *node_dev, void __user *argp)
{
	struct tracking_dev *trk_dev;
	int ret = 0;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_acpi_len_get)
			ret = trk_dev->ops->tracking_acpi_len_get(trk_dev->dev,
								  argp);
		if (ret)
			return ret;
	}
	return 0;
}

static long handle_acpi_len_cmd(struct tracking_node_dev *node_dev,
				void __user *argp)
{
	int ret = node_acpi_len_cmd(node_dev, argp);
	if (ret) {
		pr_err("node_acpi_len get failed: %d\n", ret);
		return ret;
	}
	return 0;
}

int node_iomem_len_cmd(struct tracking_node_dev *node_dev, void __user *argp)
{
	struct tracking_dev *trk_dev;
	int ret = 0;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_iomem_len_get)
			ret = trk_dev->ops->tracking_iomem_len_get(trk_dev->dev,
								   argp);
		if (ret)
			return ret;
	}
	return 0;
}

static long handle_iomem_len_cmd(struct tracking_node_dev *node_dev,
				 void __user *argp)
{
	int ret = node_iomem_len_cmd(node_dev, argp);
	if (ret) {
		pr_err("node_iomem_len get failed: %d\n", ret);
		return ret;
	}
	return 0;
}

int node_acpi_mem_cmd(struct tracking_node_dev *node_dev, void __user *argp)
{
	struct tracking_dev *trk_dev;
	int ret = 0;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_acpi_mem_get)
			ret = trk_dev->ops->tracking_acpi_mem_get(trk_dev->dev,
								  argp);
		if (ret)
			return ret;
	}
	return 0;
}

static long handle_acpi_mem_cmd(struct tracking_node_dev *node_dev,
				void __user *argp)
{
	int ret = node_acpi_mem_cmd(node_dev, argp);
	if (ret) {
		pr_err("node acpi_mem get failed: %d\n", ret);
		return ret;
	}
	return 0;
}

int node_iomem_cmd(struct tracking_node_dev *node_dev, void __user *argp)
{
	struct tracking_dev *trk_dev;
	int ret = 0;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_iomem_get)
			ret = trk_dev->ops->tracking_iomem_get(trk_dev->dev,
							       argp);
		if (ret)
			return ret;
	}
	return 0;
}

static long handle_iomem_cmd(struct tracking_node_dev *node_dev,
			     void __user *argp)
{
	int ret = node_iomem_cmd(node_dev, argp);
	if (ret) {
		pr_err("node iomem get failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static long node_cdev_user_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	struct tracking_node_dev *node_dev = file->private_data;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case SMAP_IOCTL_TRACKING_CMD:
		return handle_tracking_cmd(node_dev, arg);
	case SMAP_IOCTL_MODE_SET_CMD:
		return handle_mode_set_cmd(node_dev, arg);
	case SMAP_IOCTL_PAGE_SIZE_SET_CMD:
		return handle_page_size_set_cmd(node_dev, arg);
	case SMAP_IOCTL_REINIT_NODE_CMD:
		return handle_reinit_node_cmd(node_dev);
	case SMAP_IOCTL_RAM_CHANGE:
		return handle_ram_change_cmd(node_dev, argp);
	case SMAP_IOCTL_ACPI_LEN:
		return handle_acpi_len_cmd(node_dev, argp);
	case SMAP_IOCTL_IOMEM_LEN:
		return handle_iomem_len_cmd(node_dev, argp);
	case SMAP_IOCTL_ACPI_ADDR:
		return handle_acpi_mem_cmd(node_dev, argp);
	case SMAP_IOCTL_IOMEM_ADDR:
		return handle_iomem_cmd(node_dev, argp);
	default:
		pr_err("not support ioctl cmd! %d\n", cmd);
		return -EINVAL;
	}
}

int node_trk_data_read(struct tracking_node_dev *node_dev, void *buffer,
		       u32 length)
{
	struct tracking_dev *trk_dev;
	int result = 0;

	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->ops && trk_dev->ops->tracking_read) {
			result = trk_dev->ops->tracking_read(trk_dev->dev,
							     buffer, length);
			if (result == 0)
				return -EIO;
		}
	}
	return result;
}

#endif

ssize_t node_cdev_read_iter(struct kiocb *iocb, struct iov_iter *iov)
{
	struct file *filp = iocb->ki_filp;
	struct tracking_node_dev *node_dev = filp->private_data;
	u32 length = iov->kvec->iov_len;
	void *buffer = iov->kvec->iov_base;

	if (iov->nr_segs != DEFAULT_NR_SEG) {
		return -EINVAL;
	}
	return node_trk_data_read(node_dev, buffer, length);
}

static const struct file_operations node_dev_fops = {
	.owner = THIS_MODULE,
	.open = node_cdev_open,
	.release = node_cdev_release,
	.unlocked_ioctl = node_cdev_user_ioctl,
	.read_iter = node_cdev_read_iter,
};

static int init_tracking_device(struct tracking_node_dev *node_dev, int node_id)
{
	int result;

	node_dev->target_node = node_id;
	INIT_LIST_HEAD(&node_dev->dev_list);
	device_initialize(&node_dev->cdev_device);
	node_dev->device = &node_dev->cdev_device;
	node_dev->device->devt =
		MKDEV(MAJOR(tracking_chr_devt), (u32)node_dev->target_node);
	node_dev->device->class = tracking_class;
	node_dev->device->parent = NULL;
	dev_set_drvdata(node_dev->device, node_dev);

	result = dev_set_name(node_dev->device, "smap_node%d",
			      node_dev->target_node);
	if (result) {
		return result;
	}

	cdev_init(&node_dev->cdev, &node_dev_fops);
	node_dev->cdev.owner = THIS_MODULE;
	result = cdev_device_add(&node_dev->cdev, node_dev->device);
	if (result) {
		pr_err("add smap node%d failed\n", node_dev->target_node);
		return result;
	}

	return 0;
}

static void register_tracking_device(struct tracking_node_dev *node_dev,
				     int node_id, struct tracking_dev *dev)
{
	list_add_tail(&node_dev->list, &trk_core_ctrl->node_cdev);
	dev->trk_node_device = node_dev;
	set_bit(node_id, &trk_core_ctrl->node_bitmap);
}

static int tracking_device_probe(struct tracking_dev *dev)
{
	int result;
	struct tracking_node_dev *node_device;
	int node_id = dev->target_node;

	if (!test_bit(node_id,
		      &trk_core_ctrl->node_bitmap)) { /* register cdev */
		struct tracking_node_dev *node_dev =
			kzalloc(sizeof(*node_dev), GFP_KERNEL);
		if (!node_dev)
			return -ENOMEM;
		result = init_tracking_device(node_dev, node_id);
		if (result) {
			goto free_node_dev;
		}

		register_tracking_device(node_dev, node_id, dev);
		goto init_ok;
free_node_dev:
		kfree(node_dev);
		return result;
	}

init_ok:
	list_for_each_entry(node_device, &trk_core_ctrl->node_cdev, list) {
		if (node_device->target_node == dev->target_node) {
			list_add_tail(&dev->list, &node_device->dev_list);
			dev->trk_node_device = node_device;
		}
	}

	return 0;
}

static int node_tracking_reinit_actc_buffer(struct tracking_node_dev *node_dev,
					    int nid)
{
	struct tracking_dev *trk_dev;
	int ret = 0;
	list_for_each_entry(trk_dev, &node_dev->dev_list, list) {
		if (trk_dev->target_node != nid)
			continue;
		if (trk_dev->ops && trk_dev->ops->tracking_reinit_actc_buffer)
			ret = trk_dev->ops->tracking_reinit_actc_buffer(
				trk_dev->dev);
		if (ret)
			return ret;
	}
	return ret;
}

int tracking_core_reinit_actc_buffer(int nid)
{
	struct tracking_node_dev *node_device;
	int ret = 0;

	list_for_each_entry(node_device, &trk_core_ctrl->node_cdev, list) {
		if (node_device->target_node == nid) {
			ret = node_tracking_reinit_actc_buffer(node_device,
							       nid);
			if (ret)
				return ret;
		}
	}
	return 0;
}

EXPORT_SYMBOL(tracking_core_reinit_actc_buffer);

static void tracking_device_remove(struct tracking_dev *dev)
{
	struct tracking_node_dev *node_dev = dev->trk_node_device;
	int node_id = dev->target_node;

	list_del(&dev->list);

	if (list_empty(&node_dev->dev_list)) {
		cdev_device_del(&node_dev->cdev, &node_dev->cdev_device);
		list_del(&node_dev->list);
		clear_bit(node_id, &trk_core_ctrl->node_bitmap);
		kfree(node_dev);
	}
}

static struct tracking_driver tracking_device_driver = {
	.probe = tracking_device_probe,
	.remove = tracking_device_remove,
	.match_always = 1
};

static int __init tracking_core_init(void)
{
	int result = -ENOMEM;

	tracking_class = class_create("tracking");
	if (IS_ERR(tracking_class))
		return PTR_ERR(tracking_class);

	result = tracking_bus_init();
	if (result)
		goto destory_class;

	result = tracking_driver_register(&tracking_device_driver);
	if (result)
		goto uinit_bus;

	result = alloc_chrdev_region(&tracking_chr_devt, 0, MAX_NODE_NUM,
				     "tracking");
	if (result < 0)
		goto unregister_driver;

	trk_core_ctrl = kzalloc(sizeof(*trk_core_ctrl), GFP_KERNEL);
	if (!trk_core_ctrl) {
		result = -ENOMEM;
		goto free_chrdev;
	}

	INIT_LIST_HEAD(&trk_core_ctrl->node_cdev);

	return 0;

free_chrdev:
	unregister_chrdev_region(tracking_chr_devt, MAX_NODE_NUM);
unregister_driver:
	tracking_driver_unregister(&tracking_device_driver);
uinit_bus:
	tracking_bus_exit();
destory_class:
	class_destroy(tracking_class);

	return result;
}

static void __exit tracking_core_exit(void)
{
	unregister_chrdev_region(tracking_chr_devt, MAX_NODE_NUM);
	tracking_driver_unregister(&tracking_device_driver);
	tracking_bus_exit();
	class_destroy(tracking_class);
	kfree(trk_core_ctrl);
	trk_core_ctrl = NULL;
}

MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_LICENSE("GPL v2");
module_init(tracking_core_init);
module_exit(tracking_core_exit);
