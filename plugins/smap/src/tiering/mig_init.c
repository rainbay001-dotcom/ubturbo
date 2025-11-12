// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP: SMAP MIGRATE
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/sort.h>
#include <linux/fs.h>

#include "common.h"
#include "acpi_mem.h"
#include "iomem.h"
#include "ham_migration.h"
#include "mig_init.h"

#undef pr_fmt
#define pr_fmt(fmt) "smu_mig: " fmt

#define CMP_LT (-1)
#define CMP_EQ 0
#define CMP_GT 1
#define MAX_MIGRATE_PID_NUMA_RETRY_TIME 100

static dev_t mig_dev = 0;
static struct class *mig_dev_class;
static struct cdev smu_mig_cdev;
static struct device *smu_mig_device;

static void free_migrate_list_addr(int len, struct mig_list *mlist)
{
	int i;

	if (!mlist || len <= 0) {
		return;
	}
	for (i = 0; i < len; i++) {
		if (mlist[i].nr) {
			vfree(mlist[i].addr);
			mlist[i].addr = NULL;
		}
	}
}

static void free_migrate_list(struct mig_list **mlist)
{
	if (*mlist) {
		vfree(*mlist);
		*mlist = NULL;
	}
}

static int create_migrate_list(struct migrate_msg *msg, struct mig_list **mlist)
{
	size_t mlist_sz = msg->cnt * sizeof(struct mig_list);

	if (!msg->mig_list) {
		return -EINVAL;
	}

	/* Make sure field addr of struct mig_list is NULL */
	*mlist = vzalloc(mlist_sz);
	if (*mlist == NULL) {
		return -ENOMEM;
	}
	if (copy_from_user(*mlist, msg->mig_list, mlist_sz)) {
		vfree(*mlist);
		*mlist = NULL;
		return -EFAULT;
	}
	return 0;
}

static int init_migrate_list_addr(int len, struct mig_list *mlist)
{
	int ret;
	int i, j;
	struct mig_list *ml;

	if (len <= 0 || !mlist) {
		return -EINVAL;
	}
	for (i = 0; i < len; i++) {
		ml = &mlist[i];
		if (ml->nr == 0 || ml->nr > MAX_MIG_LIST_NR || !ml->addr) {
			return -EINVAL;
		}
	}

	for (i = 0; i < len; i++) {
		u64 *addr;
		ml = &mlist[i];

		addr = vzalloc(ml->nr * sizeof(u64));
		if (!addr) {
			ret = -ENOMEM;
			goto out;
		}

		/* Until now, mlist[i].addr contains user space pointer */
		if (copy_from_user(addr, ml->addr, ml->nr * sizeof(u64))) {
			ret = -EFAULT;
			vfree(addr);
			goto out;
		}
		/* After assignement, mlist[i].addr contains kernel space pointer */
		ml->addr = addr;
	}

	return 0;

out:
	for (j = 0; j < i; j++) {
		if (mlist[j].nr) {
			vfree(mlist[j].addr);
			mlist[j].addr = NULL;
		}
	}
	return ret;
}

static int cmp_mlist_addr_ascend(const void *a, const void *b)
{
	u64 tmp_a = *(u64 *)a;
	u64 tmp_b = *(u64 *)b;

	if (tmp_a == tmp_b) {
		return CMP_EQ;
	}
	return tmp_a < tmp_b ? CMP_LT : CMP_GT;
}

/*
 * convert_migrate_list - convert page position to physical address
 */
static int convert_migrate_list(int len, struct mig_list *mlist)
{
	int i, ret;

	if (!mlist) {
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		struct mig_list *ml = &mlist[i];

		/* Sort ml->addr to accelerate conversion */
		sort(ml->addr, ml->nr, sizeof(u64), cmp_mlist_addr_ascend,
		     NULL);

		ret = convert_pos_to_paddr_sorted(ml->pid, ml->from, ml->nr,
						  ml->addr);
		if (ret) {
			pr_err("convert pid %d migrate list from %d to %d failed: %d\n",
			       ml->pid, ml->from, ml->to, ret);
			return ret;
		}
	}
	return 0;
}

static int build_migrate_list(struct migrate_msg *msg, struct mig_list **mlist)
{
	int ret;

	if (!msg) {
		pr_err("build_migrate_list msg is NULL\n");
		return -EINVAL;
	}

	ret = create_migrate_list(msg, mlist);
	if (ret) {
		pr_err("build_migrate_list create failed: %d\n", ret);
		return ret;
	}
	ret = init_migrate_list_addr(msg->cnt, *mlist);
	if (ret) {
		pr_err("build_migrate_list init failed: %d\n", ret);
		free_migrate_list(mlist);
		return ret;
	}
	ret = convert_migrate_list(msg->cnt, *mlist);
	if (ret) {
		pr_err("build_migrate_list convert failed: %d\n", ret);
		free_migrate_list_addr(msg->cnt, *mlist);
		free_migrate_list(mlist);
	}
	return ret;
}

static bool is_migrate_msg_valid(struct migrate_msg *msg)
{
	int max_cnt = smap_pgsize == HUGE_PAGE ? MAX_2M_MIGMSG_CNT :
						 MAX_4K_MIGMSG_CNT;
	int page_size = smap_pgsize == HUGE_PAGE ? TWO_MEGA_SIZE : PAGE_SIZE;

	if (msg->cnt <= 0 || msg->cnt > max_cnt) {
		pr_err("migrate msg invalid cnt: %d\n", msg->cnt);
		return false;
	}
	if (msg->mul_mig.page_size != page_size) {
		pr_err("migrate msg page size invalid page_size: %d\n",
		       msg->mul_mig.page_size);
		return false;
	}
	if (msg->mul_mig.is_mul_thread &&
	    (msg->mul_mig.nr_thread <= 1 ||
	     msg->mul_mig.nr_thread > MAX_NR_MIGRATE_THREADS)) {
		pr_err("migrate msg invalid nr_thread: %d\n",
		       msg->mul_mig.nr_thread);
		return false;
	}
	return true;
}

static int __ioctl_migrate(void __user *argp)
{
	struct migrate_msg msg;
	struct mig_list *mig_list;
	int ret;
	if (copy_from_user(&msg, argp, sizeof(msg)))
		return -EFAULT;
	if (!is_migrate_msg_valid(&msg)) {
		return -EINVAL;
	}

	ret = build_migrate_list(&msg, &mig_list);
	if (ret) {
		return ret;
	}
	ret = do_migrate(&msg, mig_list);
	filter_4k_migrate_info();
	if (copy_to_user(argp, &msg, sizeof(msg)))
		pr_err("mig msg copy to user error\n");
	if (copy_to_user(msg.mig_list, mig_list,
			 msg.cnt * sizeof(struct mig_list)))
		pr_err("mig list copy to user error\n");

	free_migrate_list_addr(msg.cnt, mig_list);
	free_migrate_list(&mig_list);
	return ret;
}

static int __ioctl_migrate_numa(void __user *argp)
{
	int i, ret;
	struct migrate_numa_msg msg;
	struct migrate_numa_inner_msg i_msg;

	if (copy_from_user(&msg, argp, sizeof(msg)))
		return -EFAULT;
	pr_info("__ioctl_migrate_numa: src %d dest %d count %d\n", msg.src_nid,
		msg.dest_nid, msg.count);
	if (msg.count <= 0 || msg.count > MAX_NR_MIGNUMA)
		return -EINVAL;
	if (!is_numa_remote(msg.src_nid)) {
		pr_err("__ioctl_migrate_numa: src_nid %d invalid\n",
		       msg.src_nid);
		return -EINVAL;
	}
	if (!is_numa_remote(msg.dest_nid)) {
		pr_err("__ioctl_migrate_numa: dest_nid %d invalid\n",
		       msg.dest_nid);
		return -EINVAL;
	}

	i_msg.src_nid = msg.src_nid;
	i_msg.dest_nid = msg.dest_nid;
	i_msg.count = msg.count;

	for (i = 0; i < msg.count; i++) {
		struct pa_range *r = &(i_msg.range[i]);

		/* Assign values to i_p->pa_start and i_p->pa_end */
		ret = find_range_by_memid(msg.memids[i], &r->pa_start,
					  &r->pa_end);
		if (ret) {
			pr_err("__ioctl_migrate_numa cannot find memid %llu range: %d\n",
			       msg.memids[i], ret);
			return ret;
		}

		if (smap_is_remote_addr_valid(msg.src_nid, r->pa_start,
					      r->pa_end)) {
			pr_err("__ioctl_migrate_numa range mismatch with node%d\n",
			       msg.src_nid);
			return -EINVAL;
		}
	}
	if (smap_migrate_numa(&i_msg)) {
		return -ENOMEM;
	}
	return 0;
}

static int check_mig_msg(struct migrate_pid_remote_numa_msg *msg)
{
	if (!is_numa_remote(msg->src_nid)) {
		pr_err("__ioctl_migrate_pid_remote_numa: src_nid %d invalid\n",
		       msg->src_nid);
		return -EINVAL;
	}
	if (!is_numa_remote(msg->dest_nid)) {
		pr_err("__ioctl_migrate_pid_remote_numa: dest_nid %d invalid\n",
		       msg->dest_nid);
		return -EINVAL;
	}
	if (msg->dest_nid == msg->src_nid) {
		pr_err("__ioctl_migrate_pid_remote_numa: dest_nid == src_nid\n");
		return -EINVAL;
	}
	if (msg->pid_cnt <= 0 || msg->pid_cnt > get_max_pid_cnt()) {
		pr_err("__ioctl_migrate_pid_remote_numa: pid_cnt:%d is invalid\n",
		       msg->pid_cnt);
		return -EINVAL;
	}
	if (!msg->pid_list || !msg->mig_res_array) {
		pr_err("__ioctl_migrate_pid_remote_numa: pid_list or mig_res_array is NULL\n");
		return -EINVAL;
	}
	return 0;
}

static void walkpage_and_migrate(struct migrate_pid_remote_numa_msg *msg,
				 pid_t *pid_array, int *mig_res)
{
	int i;
	unsigned int failed_cnt;
	int successful_cnt = 0;
	int page_size = smap_pgsize == HUGE_PAGE ? PAGE_SIZE_2M : PAGE_SIZE_4K;

	int retry = MAX_MIGRATE_PID_NUMA_RETRY_TIME;
	do {
		for (i = 0; i < msg->pid_cnt; i++) {
			struct pagemapread pm = { 0 };
			if (mig_res[i] == 1) {
				continue;
			}
			failed_cnt = 0;
			pm.mig_type = REMOTE_MIGRATE;
			pm.mig_info.pid = pid_array[i];
			pm.mig_info.folios_len = get_node_page_cnt_iomem(
				msg->src_nid, page_size);
			pm.mig_info.remote_nid = msg->src_nid;
			pm.mig_info.folios = vzalloc(sizeof(struct folio *) *
						     pm.mig_info.folios_len);
			if (!pm.mig_info.folios) {
				pr_err("walkpage_and_migrate pid:%d folios malloc failed\n",
				       pid_array[i]);
				continue;
			}

			walk_pid_pagemap(&pm);
			pr_info("pid:%d migrate page count %d, from %d to %d\n",
				pid_array[i], pm.mig_info.mig_cnt, msg->src_nid,
				msg->dest_nid);
			failed_cnt = smap_migrate(pm.mig_info.folios,
						  pm.mig_info.mig_cnt,
						  msg->dest_nid, false);
			vfree(pm.mig_info.folios);
			if (failed_cnt == 0) {
				mig_res[i] = 1;
				successful_cnt++;
			}
		}

		if (successful_cnt == msg->pid_cnt) {
			return;
		}
		pr_info("migrate pids remote numa again, %d times left\n",
			retry);
	} while (retry--);

	pr_err("migrate pids remote numa failed after %d try\n",
	       MAX_MIGRATE_PID_NUMA_RETRY_TIME);
}

static int copy_to_user_mig_res(struct migrate_pid_remote_numa_msg *msg,
				void __user *argp, pid_t *pid_array,
				int *mig_res)
{
	if (copy_to_user(argp, msg, sizeof(*msg))) {
		pr_err("mig msg copy to user error\n");
		return -EFAULT;
	}

	if (copy_to_user(msg->pid_list, pid_array,
			 sizeof(pid_t) * msg->pid_cnt)) {
		pr_err("pid_list copy to user error\n");
		return -EFAULT;
	}

	if (copy_to_user(msg->mig_res_array, mig_res,
			 sizeof(int) * msg->pid_cnt)) {
		pr_err("mig_res_array copy to user error\n");
		return -EFAULT;
	}

	return 0;
}

static int __ioctl_migrate_pid_remote_numa(void __user *argp)
{
	int ret = 0;
	int *mig_res = NULL;
	struct migrate_pid_remote_numa_msg msg;
	pid_t *pid_array = NULL;
	pr_info("recieve __ioctl_migrate_pid_remote_numa\n");
	if (copy_from_user(&msg, argp, sizeof(msg))) {
		pr_err("__ioctl_migrate_pid_remote_numa: copy msg from user failed\n");
		return -EFAULT;
	}
	if (check_mig_msg(&msg)) {
		pr_err("__ioctl_migrate_pid_remote_numa: check msg validity failed\n");
		return -EINVAL;
	}

	pid_array = kzalloc(sizeof(pid_t) * msg.pid_cnt, GFP_KERNEL);
	if (!pid_array) {
		pr_err("__ioctl_migrate_pid_remote_numa: pid_array malloc failed\n");
		return -ENOMEM;
	}
	if (copy_from_user(pid_array, msg.pid_list,
			   sizeof(pid_t) * msg.pid_cnt)) {
		pr_err("__ioctl_migrate_pid_remote_numa: copy pid_list from user failed\n");
		kfree(pid_array);
		return -EFAULT;
	}

	mig_res = kzalloc(msg.pid_cnt * sizeof(int), GFP_KERNEL);
	if (!mig_res) {
		pr_err("__ioctl_migrate_pid_remote_numa: cannot allocate memory for mig_res\n");
		kfree(pid_array);
		return -ENOMEM;
	}

	walkpage_and_migrate(&msg, pid_array, mig_res);

	ret = copy_to_user_mig_res(&msg, argp, pid_array, mig_res);
	if (ret) {
		pr_err("__ioctl_migrate_pid_remote_numa: copy to user failed, ret %d\n",
		       ret);
	}

	kfree(mig_res);
	kfree(pid_array);
	return ret;
}

static int __ioctl_check_pagesize(void __user *argp)
{
	uint32_t pageType;
	if (copy_from_user(&pageType, argp, sizeof(uint32_t))) {
		return -EFAULT;
	}
	return pageType == smap_pgsize ? 0 : -EINVAL;
}

static long smu_mig_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int rc = -EINVAL;
	if (_IOC_TYPE(cmd) != SMAP_MIG_MAGIC)
		goto out;
	switch (cmd) {
	case SMAP_MIG_MIGRATE: {
		rc = __ioctl_migrate(argp);
		break;
	}
	case SMAP_CHECK_PAGESIZE: {
		rc = __ioctl_check_pagesize(argp);
		break;
	}
	case SMAP_MIG_MIGRATE_NUMA: {
		rc = __ioctl_migrate_numa(argp);
		break;
	}
	case SMAP_MIG_PID_REMOTE_NUMA: {
		rc = __ioctl_migrate_pid_remote_numa(argp);
		break;
	}
	default:
		rc = -ENOTTY;
	}

out:
	return rc;
}

static int smu_mig_open(struct inode *inode, struct file *file);
static int smu_mig_ioctl_release(struct inode *inode, struct file *file);
static long smu_mig_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg);

static struct file_operations smu_mig_fops = {
	.owner = THIS_MODULE,
	.open = smu_mig_open,
	.unlocked_ioctl = smu_mig_ioctl,
	.release = smu_mig_ioctl_release,
};

static int smu_mig_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int smu_mig_ioctl_release(struct inode *inode, struct file *file)
{
	return 0;
}

int init_mig_dev(void)
{
	int rc = alloc_chrdev_region(&mig_dev, BASE_MINOR, NR_MINOR,
				     SMAP_MIG_DEV);
	if (rc < 0) {
		pr_err("Cannot allocate mig major number\n");
		return rc;
	}
	cdev_init(&smu_mig_cdev, &smu_mig_fops);
	rc = cdev_add(&smu_mig_cdev, mig_dev, 1);
	if (rc) {
		pr_err("Cannot add the mig device to the system\n");
		goto err_cdev;
	}
	mig_dev_class = class_create(SMAP_MIG_CLASS);
	if (IS_ERR(mig_dev_class)) {
		pr_err("Cannot create the struct mig class\n");
		rc = PTR_ERR(mig_dev_class);
		goto err_class;
	}
	smu_mig_device = device_create(mig_dev_class, NULL, mig_dev, NULL,
				       SMAP_MIG_DEVICE);
	if (IS_ERR(smu_mig_device)) {
		pr_err("Cannot create the mig Device\n");
		rc = PTR_ERR(smu_mig_device);
		goto err_device;
	}
	return 0;

err_device:
	class_destroy(mig_dev_class);
err_class:
	cdev_del(&smu_mig_cdev);
err_cdev:
	unregister_chrdev_region(mig_dev, NR_MINOR);
	return rc;
}

void exit_mig_dev(void)
{
	device_destroy(mig_dev_class, mig_dev);
	class_destroy(mig_dev_class);
	cdev_del(&smu_mig_cdev);
	unregister_chrdev_region(mig_dev, NR_MINOR);
}

int init_migrate(void)
{
	int ret;
	ret = init_mig_dev();
	if (ret) {
		pr_err("init_mig_dev failed!, ret = %d\n", ret);
		return ret;
	}
	pr_info("init migrate success!\n");
	return ret;
}

void exit_migrate(void)
{
	exit_mig_dev();
	pr_info("exit migrate!\n");
}
