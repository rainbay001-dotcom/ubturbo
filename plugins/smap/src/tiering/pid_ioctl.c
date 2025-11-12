// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap ioctl module
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/fs.h>

#include "numa.h"
#include "work.h"
#include "iomem.h"
#include "pid_ioctl.h"

static dev_t dev = 0;
static struct class *dev_class;
static struct cdev smap_cdev;
static struct device *smap_device;

static int dup_task(unsigned long long task_id, struct list_head *head)
{
	struct migrate_back_task *t;

	list_for_each_entry(t, head, task_node) {
		if (t->task_id == task_id) {
			if (t->status == MB_TASK_ERR) {
				t->status = MB_TASK_WAITING;
				return RETRY_ID;
			}
			return DUP_ID;
		}
	}
	return NORMAL_ID;
}

static int check_duplicate_task(struct migrate_back_task *task)
{
	int task_ret;

	spin_lock(&migrate_back_task_lock);
	task_ret = dup_task(task->task_id, &migrate_back_task_list);
	if (task_ret == DUP_ID) {
		spin_unlock(&migrate_back_task_lock);
		pr_err("SmapIoctlMigrateBack task id is duplicated. Won't migrate back. task id:%llu\n",
		       task->task_id);
		return DUP_ID;
	}
	if (task_ret == RETRY_ID) {
		spin_unlock(&migrate_back_task_lock);
		return RETRY_ID;
	}
	list_add(&task->task_node, &migrate_back_task_list);
	spin_unlock(&migrate_back_task_lock);

	return 0;
}

int smap_ioctl_migrate_back(struct migrate_back_inner_msg *msg)
{
	int i, ret, task_ret;
	struct migrate_back_task *task;
	struct migrate_back_subtask *subtask, *tmp;

	pr_info("received migrate back msg\n");
	ret = 0;
	if (msg->count == 0) {
		pr_err("The migrate back message is empty.\n");
		goto err_param;
	}

	ret = -ENOMEM;
	task = init_migrate_back_task(msg->task_id);
	if (!task) {
		pr_err("failed to init migrate back task\n");
		goto err_param;
	}

	/* check duplicate task */
	ret = -EINVAL;
	task_ret = check_duplicate_task(task);
	if (task_ret == DUP_ID) {
		goto err_dup_task;
	}
	if (task_ret == RETRY_ID) {
		kfree(task);
		return 0;
	}

	for (i = 0; i < msg->count; i++) {
		ret = init_migrate_back_subtask(task, &msg->payload[i],
						&subtask);
		if (ret < 0) {
			pr_err("init migrate back subtask fail, %d->%d, %#llx-%#llx\n",
			       msg->payload[i].src_nid,
			       msg->payload[i].dest_nid,
			       msg->payload[i].pa_start,
			       msg->payload[i].pa_end);
			goto err_subtask;
		}
		list_add(&subtask->task_list, &task->subtask);
	}
	task->subtask_cnt = msg->count;
	clear_migrate_back_task();
	return start_migrate_back_work();

err_subtask:
	list_for_each_entry_safe(subtask, tmp, &task->subtask, task_list) {
		list_del(&subtask->task_list);
		kfree(subtask);
	}
	spin_lock(&migrate_back_task_lock);
	list_del(&task->task_node);
	spin_unlock(&migrate_back_task_lock);
err_dup_task:
	kfree(task);
err_param:
	return ret;
}

static int smap_open(struct inode *inode, struct file *file);
static int smap_release(struct inode *inode, struct file *file);
static long smap_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static struct file_operations smap_fops = {
	.owner = THIS_MODULE,
	.open = smap_open,
	.unlocked_ioctl = smap_ioctl,
	.release = smap_release,
};

/*
 * This function will be called when we open the Device file
 */
static int smap_open(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * This function will be called when we close the Device file
 */
static int smap_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int __ioctl_migrate_back(void __user *argp)
{
	int i, ret;
	struct migrate_back_msg mb_msg;
	struct migrate_back_inner_msg mb_imsg;

	if (copy_from_user(&mb_msg, argp, sizeof(mb_msg))) {
		pr_err("IoctlMigrateBack copy_from_user err\n");
		return -EFAULT;
	}

	if (mb_msg.count <= 0 || mb_msg.count > MAX_NR_MIGBACK) {
		pr_err("mig back msg count %d invalid.\n", mb_msg.count);
		return -EINVAL;
	}

	mb_imsg.task_id = mb_msg.task_id;
	mb_imsg.count = mb_msg.count;

	for (i = 0; i < mb_msg.count; i++) {
		struct migrate_back_payload *p = &(mb_msg.payload[i]);
		struct migrate_back_inner_payload *i_p = &(mb_imsg.payload[i]);

		if (is_node_invalid(p->src_nid)) {
			pr_err("IoctlMigrateBack %dth src_nid %d is invalid\n",
			       i, p->src_nid);
			return -EINVAL;
		}
		if (p->dest_nid != NUMA_NO_NODE &&
		    is_node_invalid(p->dest_nid)) {
			pr_err("IoctlMigrateBack %dth dest_nid %d is invalid\n",
			       i, p->dest_nid);
			return -EINVAL;
		}
		/* Assign values to i_p->pa_start and i_p->pa_end */
		ret = find_range_by_memid(p->memid, &i_p->pa_start,
					  &i_p->pa_end);
		if (ret) {
			pr_err("IoctlMigrateBack cannot find memid %llu range: %d\n",
			       p->memid, ret);
			return ret;
		}

		if (smap_is_remote_addr_valid(p->src_nid, i_p->pa_start,
					      i_p->pa_end)) {
			pr_err("IoctlMigrateBack range %#llx-%#llx mismatch with node%d\n",
			       i_p->pa_start, i_p->pa_end, p->src_nid);
			return -EINVAL;
		}

		i_p->src_nid = p->src_nid;
		i_p->dest_nid = p->dest_nid;
	}
	return smap_ioctl_migrate_back(&mb_imsg);
}

static long smap_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int rc;

	pr_info("enter smap_ioctl, cmd %u\n", _IOC_NR(cmd));
	if (_IOC_TYPE(cmd) != SMAP_MAGIC) {
		pr_err("SmapIoctl ioctl type err. type:%u\n", _IOC_NR(cmd));
		return -EINVAL;
	}

	switch (cmd) {
	case SMAP_MIGRATE_BACK:
		rc = __ioctl_migrate_back(argp);
		break;
	default:
		rc = -ENOTTY;
	}
	pr_info("exit smap_ioctl, rc %d\n", rc);

	return rc;
}

int smap_dev_init(void)
{
	int rc = alloc_chrdev_region(&dev, BASE_MINOR, NR_MINOR, SMAP_DEV);
	if (rc < 0) {
		pr_err("Cannot allocate major number\n");
		return rc;
	}

	cdev_init(&smap_cdev, &smap_fops);

	rc = cdev_add(&smap_cdev, dev, 1);
	if (rc) {
		pr_err("Cannot add the device to the system\n");
		goto err_cdev;
	}

	dev_class = class_create(SMAP_CLASS);
	if (IS_ERR(dev_class)) {
		pr_err("Cannot create the struct class\n");
		rc = PTR_ERR(dev_class);
		goto err_class;
	}

	smap_device = device_create(dev_class, NULL, dev, NULL, SMAP_DEVICE);
	if (IS_ERR(smap_device)) {
		pr_err("Cannot create the Device 1\n");
		rc = PTR_ERR(smap_device);
		goto err_device;
	}
	return 0;

err_device:
	class_destroy(dev_class);
err_class:
	cdev_del(&smap_cdev);
err_cdev:
	unregister_chrdev_region(dev, NR_MINOR);
	return rc;
}

void smap_dev_exit(void)
{
	free_all_migrate_back_task();
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&smap_cdev);
	unregister_chrdev_region(dev, NR_MINOR);
}
