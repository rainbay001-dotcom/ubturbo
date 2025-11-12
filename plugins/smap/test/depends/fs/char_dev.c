// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>

#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/major.h>

#include <linux/kobject.h>
#include <linux/cdev.h>
void cdev_init(struct cdev *cdev, const struct file_operations *fops)
{
}

int cdev_device_add(struct cdev *cdev, struct device *dev)
{
	return 0;
}

void cdev_device_del(struct cdev *cdev, struct device *dev)
{
}

int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count,
			const char *name)
{
	return 0;
}

void unregister_chrdev_region(dev_t from, unsigned count)
{
}

int cdev_add(struct cdev *p, dev_t dev, unsigned count)
{
	return 0;
}

void cdev_del(struct cdev *p)
{
}
