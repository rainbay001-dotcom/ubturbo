// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pid_namespace.h>
#include "ucache.h"

unsigned int major;
static dev_t ucache_dev;
static struct cdev ucache_cdev;
static struct class *ucache_dev_class;
static struct folio *folios[NR_TO_SCAN];

static int ucache_open(struct inode *inode, struct file *file)
{
	return 0;
}

bool is_pid_in_container(pid_t pid)
{
	struct task_struct *task;
	struct pid_namespace *pns;
	bool in_container = false;

	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!task) {
		pr_err("task not found, pid=%d\n", pid);
		goto out;
	}

	pns = task_active_pid_ns(task);
	if (pns && pns != &init_pid_ns) {
		in_container = true;
	}

out:
	rcu_read_unlock();
	return in_container;
}

static int ucache_query_migrate_success(uintptr_t arg)
{
	struct migrate_success tmp;
	memset(&tmp, 0, sizeof(tmp));
	struct migrate_success *success_rate = NULL;
	int nid = -1;

	if (copy_from_user(&tmp, (void __user *)arg, sizeof(tmp))) {
		pr_err("%s copy from user failed\n", __func__);
		return -EFAULT;
	}

	if (tmp.des_nid < 0 || tmp.des_nid >= MAX_NUMNODES ||
	    !node_online(tmp.des_nid)) {
		pr_err("invalid nid=%d\n", tmp.des_nid);
		return -EINVAL;
	}
	nid = tmp.des_nid;

	success_rate = get_migrate_success(nid);
	if (success_rate == NULL) {
		pr_err("ucache migrate_success not found, nid=%d\n", nid);
		return -EINVAL;
	}
	tmp.nr_folios = success_rate->nr_folios;
	tmp.nr_succeeded = success_rate->nr_succeeded;

	if (copy_to_user((void __user *)arg, &tmp, sizeof(tmp))) {
		pr_err("%s copy to user failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}

static int dev_ucache_migrate_folios(uintptr_t arg)
{
	int ret = 0;
	struct migrate_info migrate_info;
	memset(&migrate_info, 0, sizeof(migrate_info));
	int pid, nid, des_nid;
	unsigned int nr_folios = NR_TO_SCAN;

	if (copy_from_user(&migrate_info, (void __user *)arg,
			   sizeof(migrate_info))) {
		pr_err("%s copy from user failed\n", __func__);
		return -EFAULT;
	}

	pid = migrate_info.pid;
	if (pid < 0) {
		pr_err("invalid pid = %d\n", pid);
		return -EINVAL;
	}

	if (!is_pid_in_container(pid)) {
		pr_err("pid does not belong to a container, migration denied, pid=%d\n",
		       pid);
		return -EINVAL;
	}

	if (migrate_info.nid < 0 || migrate_info.nid >= MAX_NUMNODES ||
	    !node_online(migrate_info.nid)) {
		pr_err("invalid nid=%d\n", migrate_info.nid);
		return -EINVAL;
	}
	nid = migrate_info.nid;

	if (migrate_info.des_nid < 0 || migrate_info.des_nid >= MAX_NUMNODES ||
	    !node_online(migrate_info.des_nid)) {
		pr_err("invalid nid=%d\n", migrate_info.des_nid);
		return -EINVAL;
	}
	des_nid = migrate_info.des_nid;

	ret = ucache_scan_folios(nid, pid, folios, &nr_folios);
	if (ret != 0) {
		pr_err("ucache scan folios failed: %d\n", ret);
		return ret;
	} else if (nr_folios == 0) {
		pr_warn("ucache no folios to migrate\n");
	}

	ret = ucache_migrate_folios(des_nid, folios, nr_folios);
	if (ret) {
		pr_err("ucache migrate failed, ret = %d\n", ret);
		return ret;
	}
	return 0;
}

static long ucache_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	uintptr_t cmd_ptr = (uintptr_t)arg;
	switch (cmd) {
	case UCACHE_QUERY_MIGRATE_SUCCESS:
		ret = ucache_query_migrate_success(cmd_ptr);
		break;
	case UCACHE_SCAN_MIGRATE_FOLIOS:
		ret = dev_ucache_migrate_folios(cmd_ptr);
		break;
	default:
		pr_err(" Undefined ioctl cmd\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

struct file_operations ucache_fops = {
	.owner = THIS_MODULE,
	.open = ucache_open,
	.unlocked_ioctl = ucache_ioctl,
};

static int __init ucache_core_init(void)
{
	int ret = 0;
	struct device *dev = NULL;

	ret = alloc_chrdev_region(&ucache_dev, CHRDEV_MAION, CHRDEV_COUNT,
				  DEVICE_FILE_NAME);
	if (ret < 0) {
		pr_err("alloc_chrdev_region error\n");
		goto out;
	}
	major = MAJOR(ucache_dev);
	cdev_init(&ucache_cdev, &ucache_fops);
	ret = cdev_add(&ucache_cdev, ucache_dev, CHRDEV_COUNT);
	if (ret < 0) {
		pr_err("cdev_add error\n");
		goto cdev_add_fail;
	}

	ucache_dev_class = class_create(DEVICE_FILE_NAME);
	if (IS_ERR(ucache_dev_class)) {
		pr_err("class_create error\n");
		ret = -ENOMEM;
		goto class_create_fail;
	}

	dev = device_create(ucache_dev_class, NULL, MKDEV(major, 0), NULL,
			    DEVICE_FILE_NAME);
	if (dev == NULL) {
		pr_err("device_create error\n");
		ret = -ENOMEM;
		goto device_create_fail;
	}
	pr_info("ucache_core_init");
	return 0;

device_create_fail:
	class_destroy(ucache_dev_class);
	ucache_dev_class = NULL;
class_create_fail:
	cdev_del(&ucache_cdev);
cdev_add_fail:
	unregister_chrdev_region(ucache_dev, CHRDEV_COUNT);
out:
	return ret;
}

static void __exit ucache_core_exit(void)
{
	device_destroy(ucache_dev_class, MKDEV(major, 0));
	class_destroy(ucache_dev_class);
	ucache_dev_class = NULL;
	cdev_del(&ucache_cdev);
	unregister_chrdev_region(ucache_dev, CHRDEV_COUNT);
	pr_info("ucache_core_exit");
}

MODULE_LICENSE("GPL");
module_init(ucache_core_init);
module_exit(ucache_core_exit);
