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
#define pr_fmt(fmt) "SMAP_mig: " fmt

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

		/* Currently, mlist[i].addr contains user space pointer */
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
			pr_err("failed to convert pid %d migrate list from %d to %d, ret: %d\n",
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
		pr_err("null message passed to build migrate list\n");
		return -EINVAL;
	}

	ret = create_migrate_list(msg, mlist);
	if (ret) {
		pr_err("failed to create migrate list, ret: %d\n", ret);
		return ret;
	}
	ret = init_migrate_list_addr(msg->cnt, *mlist);
	if (ret) {
		pr_err("failed to init migrate list address, ret: %d\n", ret);
		free_migrate_list(mlist);
		return ret;
	}
	ret = convert_migrate_list(msg->cnt, *mlist);
	if (ret) {
		pr_err("failed to convert migrate list, ret: %d\n", ret);
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
		pr_err("invalid migrate message cnt: %d passed to check\n",
		       msg->cnt);
		return false;
	}
	if (msg->mul_mig.page_size != page_size) {
		pr_err("invalid page size: %d passed to check\n",
		       msg->mul_mig.page_size);
		return false;
	}
	if (msg->mul_mig.is_mul_thread &&
	    (msg->mul_mig.nr_thread <= 1 ||
	     msg->mul_mig.nr_thread > MAX_NR_MIGRATE_THREADS)) {
		pr_err("invalid threads number: %d passed to check\n",
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
		pr_err("unable to copy migrate message to user space\n");
	if (copy_to_user(msg.mig_list, mig_list,
			 msg.cnt * sizeof(struct mig_list)))
		pr_err("unable to copy migrate list to user space\n");

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
	pr_info("received migrate NUMA message: source node id: %d, "
		"destination node id: %d, page count: %d\n",
		msg.src_nid, msg.dest_nid, msg.count);
	if (msg.count <= 0 || msg.count > MAX_NR_MIGNUMA)
		return -EINVAL;
	if (!is_numa_remote(msg.src_nid)) {
		pr_err("invalid source node id: %d passed to migrate NUMA\n",
		       msg.src_nid);
		return -EINVAL;
	}
	if (!is_numa_remote(msg.dest_nid)) {
		pr_err("invalid destination node id: %d passed to migrate NUMA\n",
		       msg.dest_nid);
		return -EINVAL;
	}

	i_msg.src_nid = msg.src_nid;
	i_msg.dest_nid = msg.dest_nid;
	i_msg.count = msg.count;

	for (i = 0; i < msg.count; i++) {
		struct pa_range *r = &(i_msg.range[i]);

		ret = find_range_by_memid(msg.memids[i], &r->pa_start,
					  &r->pa_end);
		if (ret) {
			pr_err("unable to find memory range of memid %llu, ret: %d\n",
			       msg.memids[i], ret);
			return ret;
		}

		if (smap_is_remote_addr_valid(msg.src_nid, r->pa_start,
					      r->pa_end)) {
			pr_err("memory range mismatch with node: %d\n",
			       msg.src_nid);
			return -EINVAL;
		}
	}
	if (smap_migrate_numa(&i_msg)) {
		return -ENOMEM;
	}
	return 0;
}

static int check_mig_msg(struct mig_payload *payloads, int len)
{
	for (int i = 0; i < len; i++) {
		if (!is_numa_remote(payloads[i].src_nid)) {
			pr_err("invalid source node id: %d passed to check params\n",
				   payloads[i].src_nid);
			return -EINVAL;
		}
		if (!is_numa_remote(payloads[i].dest_nid)) {
			pr_err("invalid destination node id: %d passed to check params\n",
				   payloads[i].dest_nid);
			return -EINVAL;
		}
		if (payloads[i].dest_nid == payloads[i].src_nid) {
			pr_err("source and destination node id should not be the same\n");
			return -EINVAL;
		}
		if (payloads[i].ratio <= 0 || payloads[i].ratio > HUNDRED) {
			pr_err("migrate ratio: %d invalid\n", payloads[i].ratio);
			return -EINVAL;
		}
	}
	return 0;
}

static void init_pm_info(struct pagemapread *pm, struct mig_payload *payload)
{
	int page_size = smap_pgsize == HUGE_PAGE ? PAGE_SIZE_2M : PAGE_SIZE_4K;
	pm->mig_type = REMOTE_MIGRATE;
	pm->mig_info.pid = payload->pid;
	pm->mig_info.folios_len = get_node_page_cnt_iomem(payload->src_nid, page_size);
	pm->mig_info.remote_nid = payload->src_nid;
}

static void walkpage_and_migrate(struct mig_payload *payloads, int len, int *mig_res)
{
	int i;
	unsigned int failed_cnt;
	u64 mig_cnt;
	int successful_cnt = 0;

	int retry = MAX_MIGRATE_PID_NUMA_RETRY_TIME;
	do {
		for (i = 0; i < len; i++) {
			struct pagemapread pm = { 0 };
			if (mig_res[i] == 1) {
				continue;
			}
			failed_cnt = 0;
			init_pm_info(&pm, &payloads[i]);
			pm.mig_info.folios = vzalloc(sizeof(struct folio *) *
						     pm.mig_info.folios_len);
			if (!pm.mig_info.folios) {
				pr_err("unable to allocate memory for folio list to walk page range\n");
				continue;
			}

			walk_pid_pagemap(&pm);

			pr_info("pid :%d total page count:%llu", payloads[i].pid, pm.mig_info.page_cnt);
			if (payloads[i].is_ratio_mode) {
				mig_cnt = (pm.mig_info.page_cnt * payloads[i].ratio + (HUNDRED / 2)) / HUNDRED;
			} else {
				mig_cnt = smap_pgsize == HUGE_PAGE ? (payloads[i].mem_size >> KB_TO_2M) : (payloads[i].mem_size >> KB_TO_4K);
			}

			for (int j = mig_cnt; j < pm.mig_info.mig_cnt; j++) {
				folio_put(pm.mig_info.folios[j]);
			}

			mig_cnt = MIN(mig_cnt, pm.mig_info.mig_cnt);
			pr_info("pid:%d migrate page count: %llu, from: %d to: %d\n",
					payloads[i].pid, mig_cnt, payloads[i].src_nid, payloads[i].dest_nid);

			failed_cnt = smap_migrate(pm.mig_info.folios, mig_cnt,
						   payloads[i].dest_nid, false);

			vfree(pm.mig_info.folios);
			if (failed_cnt == 0) {
				mig_res[i] = 1;
				payloads[i].success_cnt = mig_cnt;
				successful_cnt++;
			}
		}

		if (successful_cnt == len) {
			return;
		}
	} while (retry--);
	pr_err("migrate pids remote numa failed after %d try\n",
	       MAX_MIGRATE_PID_NUMA_RETRY_TIME);
}

static int copy_to_user_mig_res(struct migrate_pid_remote_numa_msg *msg,
				void __user *argp, struct mig_payload *payload,
				int *mig_res)
{
	if (copy_to_user(argp, msg, sizeof(*msg))) {
		pr_err("failed to copy migrate message to user space\n");
		return -EFAULT;
	}

	if (copy_to_user(msg->payloads, payload,
			 sizeof(struct mig_payload) * msg->pid_cnt)) {
		pr_err("failed to copy pid list to user space\n");
		return -EFAULT;
	}

	if (copy_to_user(msg->mig_res_array, mig_res,
			 sizeof(int) * msg->pid_cnt)) {
		pr_err("failed to copy migrate result array to user space\n");
		return -EFAULT;
	}

	return 0;
}

static int __ioctl_migrate_pid_remote_numa(void __user *argp)
{
	int ret = 0;
	int *mig_res = NULL;
	struct migrate_pid_remote_numa_msg msg;
	struct mig_payload *payloads = NULL;
	if (copy_from_user(&msg, argp, sizeof(msg))) {
		pr_err("failed to copy migrate pid remote NUMA message from user space\n");
		return -EFAULT;
	}
	if (msg.pid_cnt <= 0 || msg.pid_cnt > get_max_pid_cnt()) {
		pr_err("invalid pid count passed to check params\n");
		return -EINVAL;
	}
	if (!msg.payloads || !msg.mig_res_array) {
		pr_err("null pid payloads or null migrate result array passed to check params\n");
		return -EINVAL;
	}

	payloads = kzalloc(sizeof(struct mig_payload) * msg.pid_cnt, GFP_KERNEL);
	if (!payloads) {
		pr_err("unable to allocate memory for pid array\n");
		return -ENOMEM;
	}
	if (copy_from_user(payloads, msg.payloads,
			   sizeof(struct mig_payload) * msg.pid_cnt)) {
		pr_err("failed to copy pid array from user space\n");
		kfree(payloads);
		return -EFAULT;
	}

	if (check_mig_msg(payloads, msg.pid_cnt)) {
		pr_err("invalid params passed to migrate pid remote NUMA\n");
		return -EINVAL;
	}

	mig_res = kzalloc(msg.pid_cnt * sizeof(int), GFP_KERNEL);
	if (!mig_res) {
		pr_err("unable to allocate memory for migrate result\n");
		kfree(payloads);
		return -ENOMEM;
	}

	walkpage_and_migrate(payloads, msg.pid_cnt, mig_res);

	ret = copy_to_user_mig_res(&msg, argp, payloads, mig_res);
	if (ret)
		pr_err("failed to copy migrate result to user space\n");

	kfree(mig_res);
	kfree(payloads);
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
		pr_err("unable to allocate migrate device major number\n");
		return rc;
	}
	cdev_init(&smu_mig_cdev, &smu_mig_fops);
	rc = cdev_add(&smu_mig_cdev, mig_dev, 1);
	if (rc) {
		pr_err("unable to add migrate device\n");
		goto err_cdev;
	}
	mig_dev_class = class_create(SMAP_MIG_CLASS);
	if (IS_ERR(mig_dev_class)) {
		pr_err("unable to create SMAP migrate class\n");
		rc = PTR_ERR(mig_dev_class);
		goto err_class;
	}
	smu_mig_device = device_create(mig_dev_class, NULL, mig_dev, NULL,
				       SMAP_MIG_DEVICE);
	if (IS_ERR(smu_mig_device)) {
		pr_err("unable to create migrate device\n");
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
		pr_err("failed to init migrate device, ret: %d\n", ret);
		return ret;
	}
	pr_info("init SMAP migrate successfully\n");
	return ret;
}

void exit_migrate(void)
{
	exit_mig_dev();
	pr_info("exit SMAP migrate\n");
}
