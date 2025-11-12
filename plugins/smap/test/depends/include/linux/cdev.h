/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_CDEV_H
#define _LINUX_CDEV_H

#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/list.h>
#include <linux/device.h>
struct cdev {
	struct module *owner;
};

#ifdef __cplusplus
extern "C" {
#endif
void cdev_init(struct cdev *, const struct file_operations *);
int cdev_device_add(struct cdev *cdev, struct device *dev);
void cdev_device_del(struct cdev *cdev, struct device *dev);

int cdev_add(struct cdev *, dev_t, unsigned);
void cdev_del(struct cdev *p);
#ifdef __cplusplus
}
#endif

#endif //_LINUX_CDEV_H
