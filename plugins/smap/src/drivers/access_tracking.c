// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: smap access_tracking module
 */

#include <asm/types.h>
#include <asm/kvm_pgtable.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kprobes.h>
#include <linux/rcupdate.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/hugetlb.h>
#include <linux/mmzone.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>

#include "check.h"
#include "access_iomem.h"
#include "access_ioctl.h"
#include "access_tracking_wrapper.h"
#include "memory_notifier.h"
#include "access_pid.h"
#include "accessed_bit.h"
#include "access_pid.h"
#include "hist_ops.h"
#include "access_tracking.h"

#define DEFAULT_PERIOD_MS 50
#define MAX_SCAN_TIME 100000 /* 100s */
#define MS_TO_US 1000
#define DELAY_BUFFER_MS 8

static void work_func(struct work_struct *work);
int calc_access_len(struct access_tracking_dev *adev);

#define to_accessbit_dev(n) container_of(n, struct access_tracking_dev, ldev)
#define to_delay_work(n) container_of(n, struct delayed_work, work)
#define delay_work_to_ap(n) container_of(n, struct access_pid, scan_work)
#undef pr_fmt
#define pr_fmt(fmt) "access-bit: " fmt

unsigned int smap_scene = NORMAL_SCENE;
module_param(smap_scene, uint, S_IRUGO);
MODULE_PARM_DESC(smap_scene, "smap use scene: 0 for HCCS, 1 for UB_QEMU");

unsigned int enable_hist = DISABLE_HIST;
module_param(enable_hist, uint, S_IRUGO);
MODULE_PARM_DESC(enable_hist, "smap hist disable: 0, smap hist enable: 1");

LIST_HEAD(access_dev);
u8 access_page_size = PAGE_MODE_2M;

u32 g_pagesize_huge;
EXPORT_SYMBOL(g_pagesize_huge);

ktime_t calc_time_us(ktime_t start_time)
{
	ktime_t cur_time, time_us;
	cur_time = ktime_get();
	time_us = ktime_to_us(ktime_sub(cur_time, start_time));
	return time_us;
}
EXPORT_SYMBOL(calc_time_us);

void cancel_ap_scan_work(struct access_pid *ap)
{
	if (ap && ap->scan_work.work.func) {
		cancel_delayed_work_sync(&ap->scan_work);
	}
}

void submit_one_work(struct access_pid *ap)
{
	struct access_tracking_dev *adev_head = get_first_access_dev();
	pr_debug("submit_one_work: pid=%d, delay=%dms\n", ap->pid,
		 ap->scan_time);
	/* check if work was already initialized */
	cancel_ap_scan_work(ap);
	init_completion(&ap->work_done);
	INIT_DELAYED_WORK(&ap->scan_work, work_func);
	queue_delayed_work(adev_head->scanq, &ap->scan_work,
			   msecs_to_jiffies(ap->scan_time));
}

static void submit_scan_works(struct access_tracking_dev *adev)
{
	struct access_pid *ap;
	/* only the first adev in the list submits scan works */
	if (adev != get_first_access_dev()) {
		return;
	}
	down_read(&ap_data.lock);
	list_for_each_entry(ap, &ap_data.list, node) {
		ap->cur_times = 0;
		submit_one_work(ap);
	}
	up_read(&ap_data.lock);
}

static void wait_scan_works(struct access_tracking_dev *adev)
{
	struct access_pid *ap;
	struct access_tracking_dev *adev_head = get_first_access_dev();
	if (adev != adev_head) {
		return;
	}
	down_read(&ap_data.lock);
	list_for_each_entry(ap, &ap_data.list, node) {
		if (wait_for_completion_killable(&ap->work_done)) {
			up_read(&ap_data.lock);
			return;
		}
	}
	up_read(&ap_data.lock);
}

static int create_scan_workqueue(void)
{
	struct access_tracking_dev *adev = get_first_access_dev();
	adev->scanq = alloc_workqueue("accessbit_workq", WQ_CPU_INTENSIVE,
				      WQ_MAX_THREADS);
	if (!adev->scanq) {
		pr_err("unable to init access bit workqueue\n");
		return -ENOMEM;
	}
	return 0;
}

static void destroy_scan_workqueue(void)
{
	struct access_pid *ap;
	struct access_tracking_dev *adev = get_first_access_dev();
	down_read(&ap_data.lock);
	list_for_each_entry(ap, &ap_data.list, node) {
		cancel_ap_scan_work(ap);
	}
	up_read(&ap_data.lock);
	flush_workqueue(adev->scanq);
	destroy_workqueue(adev->scanq);
}

static inline void init_actc_data(struct access_tracking_dev *adev)
{
	size_t len = adev->page_count * sizeof(actc_t);

	if (adev->access_bit_actc_data)
		memset(adev->access_bit_actc_data, 0, len);
}

static void access_tracking_enable(struct device *ldev)
{
	struct access_tracking_dev *adev = to_accessbit_dev(ldev);
	if (adev->is_hist)
		return;
	down_write(&adev->buffer_lock);
	init_actc_data(adev);
	up_write(&adev->buffer_lock);
	submit_scan_works(adev);
}

static void access_tracking_disable(struct device *ldev)
{
	struct access_tracking_dev *adev = to_accessbit_dev(ldev);
	if (adev->is_hist)
		return;
	wait_scan_works(adev);
}

static u64 calc_access_len_v2(struct access_tracking_dev *adev)
{
	int page_size = get_page_size(adev);
	u64 page_count;
	if (adev->node >= nr_local_numa) {
		page_count = get_node_page_cnt_iomem(adev->node, page_size);
	} else {
		page_count = get_node_actc_len(adev->node, page_size);
	}
	pr_debug("access device of node: %d, page amount: %llu\n", adev->node,
		 page_count);

	return page_count;
}

static int access_tracking_mode_set(struct device *ldev, u8 mode)
{
	struct access_tracking_dev *adev = to_accessbit_dev(ldev);

	if (!(mode == ACCESS_MODE_AND || mode == ACCESS_MODE_SUM ||
	      mode == ACCESS_MODE_OR)) {
		pr_err("invalid access mode %u passed to access tracking set tracking mode\n",
		       mode);
		return -EPERM;
	}

	init_actc_data(adev);
	return 0;
}

static void access_print_acpi_mem(void)
{
#ifdef DEBUG
	struct acpi_mem_segment *mem;
	list_for_each_entry(mem, &acpi_mem.mem, segment) {
		pr_debug("[%d] %#llx-%#llx\n", mem->node, mem->start, mem->end);
	}
#endif
}

static void actc_buffer_deinit(struct access_tracking_dev *adev)
{
	adev->page_count = 0;
	if (adev->access_bit_actc_data) {
		vfree(adev->access_bit_actc_data);
		adev->access_bit_actc_data = NULL;
	}
}

static int actc_buffer_reinit(struct access_tracking_dev *adev)
{
	u64 page_count;

	access_print_acpi_mem();
	page_count = calc_access_len_v2(adev);
	if (adev->page_count == page_count) {
		init_actc_data(adev);
		return 0;
	}
	pr_debug(
		"page amount of tracking device on node %d has been changed from %llu to %llu\n",
		adev->node, adev->page_count, page_count);
	actc_buffer_deinit(adev);
	if (page_count == 0) {
		return 0;
	}

	adev->access_bit_actc_data = vzalloc(page_count * sizeof(actc_t));
	if (!adev->access_bit_actc_data) {
		return -ENOMEM;
	}
	adev->page_count = page_count;
	return 0;
}

static int access_tracking_reinit_actc_buffer(struct device *ldev)
{
	struct access_tracking_dev *adev = to_accessbit_dev(ldev);
	int ret;
	down_write(&adev->buffer_lock);
	ret = actc_buffer_reinit(adev);
	up_write(&adev->buffer_lock);
	return ret;
}

static int access_tracking_set_page_size(struct device *ldev,
					 u8 page_size_index)
{
	int ret;
	struct access_tracking_dev *adev = to_accessbit_dev(ldev);

	if (page_size_index != PAGE_MODE_2M && page_size_index != PAGE_MODE_4K)
		return -EINVAL;

	down_write(&adev->buffer_lock);
	access_page_size = adev->page_size_mode = page_size_index;
	ret = actc_buffer_reinit(adev);
	if (ret) {
		up_write(&adev->buffer_lock);
		return ret;
	}
	up_write(&adev->buffer_lock);
	pr_info("set tracking page size to %u\n", page_size_index);
	return 0;
}

static int access_tracking_ram_change(struct device *ldev, void __user *argp)
{
	int is_change = (ram_changed()) ? 1 : 0;

	if (copy_to_user(argp, &is_change, sizeof(int))) {
		pr_err("unable to copy ram changed info to user space\n");
		return -EFAULT;
	}
	return 0;
}

static struct tracking_operations access_tracking_ops = {
	.tracking_enable = access_tracking_enable,
	.tracking_disable = access_tracking_disable,
	.tracking_ram_change = access_tracking_ram_change,
	.tracking_reinit_actc_buffer = access_tracking_reinit_actc_buffer,
	.tracking_set_page_size = access_tracking_set_page_size,
	.tracking_mode_set = access_tracking_mode_set,
};

int calc_access_len(struct access_tracking_dev *adev)
{
	int page_size = get_page_size(adev);
	adev->page_count = get_node_actc_len(adev->node, page_size);

	return 0;
}

static int actc_buffer_init(struct access_tracking_dev *adev)
{
	adev->page_count = calc_access_len_v2(adev);
	pr_info("tracking device of node: %d, page amount: %llu\n", adev->node,
		adev->page_count);
	if (adev->page_count == 0) {
		adev->access_bit_actc_data = NULL;
		return 0;
	}

	adev->access_bit_actc_data = vzalloc(adev->page_count * sizeof(actc_t));
	if (!adev->access_bit_actc_data) {
		pr_err("unable to allocate memory for access bit ACTC buffer\n");
		return -ENOMEM;
	}
	return 0;
}

static int access_tracking_add(void)
{
	int ret;
	int devno;
	struct access_tracking_dev *adev, *n;
	int access_devices_cnt = enable_hist ? nr_local_numa : SMAP_MAX_NUMNODES;

	for (devno = 0; devno < access_devices_cnt; devno++) {
		adev = kzalloc(sizeof(struct access_tracking_dev), GFP_KERNEL);
		if (!adev) {
			pr_err("unable to allocate memory for access tracking device\n");
			goto put_dev;
		}

		adev->node = devno;
		adev->page_size_mode = PAGE_MODE_2M;
		adev->is_hist = false;

		ret = actc_buffer_init(adev);
		if (ret) {
			pr_err("unable to init ACTC buffer\n");
			goto adev_free;
		}

		init_rwsem(&adev->buffer_lock);
		device_initialize(&adev->ldev);
		adev->ldev.release = access_tracking_dev_release;
		ret = dev_set_name(&adev->ldev, "access_bit%d", devno);
		if (ret) {
			pr_err("unable to set name for access bit device, ret: %d\n",
			       ret);
			goto buff_deinit;
		}

		ret = device_add(&adev->ldev);
		if (ret) {
			pr_err("unable to add access bit device\n");
			goto buff_deinit;
		}

		adev->tracking_dev = tracking_dev_add(
			&adev->ldev, &access_tracking_ops, adev->node);
		if (!adev->tracking_dev) {
			pr_err("unable to add tracking device\n");
			goto attr_del;
		}
		list_add_tail(&adev->list, &access_dev);
	}
	return 0;

attr_del:
	device_del(&adev->ldev);
buff_deinit:
	put_device(&adev->ldev);
	actc_buffer_deinit(adev);
adev_free:
	kfree(adev);
	adev = NULL;
put_dev:
	list_for_each_entry_safe(adev, n, &access_dev, list) {
		tracking_dev_remove(adev->tracking_dev);
		actc_buffer_deinit(adev);
		device_unregister(&adev->ldev);
		kfree(adev);
	}

	return -ENODEV;
}

static void adev_buffer_down_read(void)
{
	struct access_tracking_dev *adev;
	list_for_each_entry(adev, &access_dev, list) {
		down_read(&adev->buffer_lock);
	}
}

static void adev_buffer_up_read(void)
{
	struct access_tracking_dev *adev;
	list_for_each_entry(adev, &access_dev, list) {
		up_read(&adev->buffer_lock);
	}
}

static void handle_statistic_scan(struct access_pid *ap, ktime_t start_time,
    s64 scan_time, unsigned long *scan_delay_ms)
{
	unsigned long delay_buffer_ms;
	if (ap->cur_times == 1) {
		delay_buffer_ms = DELAY_BUFFER_MS;
	} else {
		delay_buffer_ms = ktime_to_ms(ktime_sub(start_time, ap->last_scan_end)) - ap->last_scan_delay_ms;
	}

	if (*scan_delay_ms < ((scan_time / MS_TO_US) + delay_buffer_ms)) {
		pr_err("pid[%d] scan cost %lums exceeded expected scan time:%lums\n", ap->pid,
			   (unsigned long)((scan_time / MS_TO_US) + delay_buffer_ms), *scan_delay_ms);
		*scan_delay_ms = 0;
	} else {
		*scan_delay_ms -= (scan_time / MS_TO_US + delay_buffer_ms);
	}
	pr_debug("pid[%d] statistic scan delay_buffer_ms :%ldms, scan_delay_ms: %ldms \n",
          ap->pid, delay_buffer_ms, *scan_delay_ms);
}

static void work_func(struct work_struct *work)
{
	int ret = 0;
	int page_size;
	struct access_tracking_dev *adev = get_first_access_dev();
	struct access_pid *ap;
	struct delayed_work *scan_work;
	ktime_t start_time, end_time;
	s64 scan_time;

	unsigned long scan_delay_ms;
	start_time = ktime_get();
	scan_work = to_delay_work(work);
	ap = delay_work_to_ap(scan_work);
	adev_buffer_down_read();
	page_size = get_page_size(adev);
	if (page_size == g_pagesize_huge) {
		ret = scan_accessed_bit_forward_vm(ap->pid, page_size,
						   ap->type);
	} else {
		ret = scan_accessed_bit_forward_mm(ap->pid, page_size,
						   ap->type);
	}
	adev_buffer_up_read();
	end_time = ktime_get();
	scan_time = ktime_to_us(ktime_sub(end_time, start_time));
	if (ret < 0) {
		pr_err("unable to scan access-flag, page amount: %llu, page size: %d, node: %d\n",
		       adev->page_count, page_size, adev->node);
	}
	ap->cur_times++;
	pr_debug("pid[%d] scan took %lldus for %dth time\n", ap->pid, scan_time, ap->cur_times);

	scan_delay_ms = ap->scan_time;
	if (ap->type == STATISTIC_SCAN) {
		handle_statistic_scan(ap, start_time, scan_time, &scan_delay_ms);
	}
	ap->last_scan_delay_ms = scan_delay_ms;
	if (ap->cur_times < ap->ntimes) {
		queue_delayed_work(adev->scanq, &ap->scan_work, msecs_to_jiffies(scan_delay_ms));
		ap->last_scan_end = ktime_get();
	} else {
		pr_debug("pid[%d] start to walk pagemap\n", ap->pid);
		if (ap->type != STATISTIC_SCAN) {
			access_walk_pagemap(ap);
		}
		complete(&ap->work_done);
	}
}

static int remote_ram_init(void)
{
	int ret;
	ret = refresh_remote_ram();
	if (ret) {
		pr_err("unable to refresh remote ram info, ret: %d\n", ret);
		return ret;
	}
	if (smap_scene != UB_QEMU_SCENE_ADVANCED) {
		if (list_empty(&remote_ram_list)) {
			pr_err("remote NUMA node not detected\n");
			return -EINVAL;
		}
		pr_info("remote NUMA node detected\n");
	}
	return 0;
}

static void release_adev(void)
{
	struct access_tracking_dev *adev;
	struct access_tracking_dev *n;

	list_for_each_entry_safe(adev, n, &access_dev, list) {
		tracking_dev_remove(adev->tracking_dev);
		device_unregister(&adev->ldev);
		vfree(adev->access_bit_actc_data);
		adev->access_bit_actc_data = NULL;
		kfree(adev);
	}
}

static int __init access_tracking_init(void)
{
	int ret = 0;

	g_pagesize_huge = PAGE_SIZE_2M;
	ret = init_acpi_mem();
	if (ret) {
		pr_err("unable to init local memory info by ACPI table, ret: %d\n",
		       ret);
		return ret;
	}
	spin_lock_init(&ham_lock);
	init_rwsem(&statistic_lock);
	ret = remote_ram_init();
	if (ret) {
		goto err_remote_ram;
	}
	memory_notifier_init();

	ret = access_ioctl_init();
	if (ret) {
		pr_err("unable to init access tracking ioctl operations\n");
		goto err_ioctl;
	}

	ret = access_tracking_add();
	if (ret) {
		pr_err("unable to add access tracking device to tracking bus\n");
		goto err_tracking_add;
	}

	if (enable_hist) {
		ret = hist_module_init();
		if (ret) {
			pr_err("unable to init hist tracking\n");
			goto err_tracking_add;
		}
	}
	if (create_scan_workqueue()) {
		goto err_tracking_add;
	}

	access_print_acpi_mem();

	pr_info("access tracking init successfully\n");
	return ret;

err_tracking_add:
	release_adev();
	access_ioctl_exit();
err_ioctl:
	memory_notifier_exit();
	release_remote_ram();
err_remote_ram:
	reset_acpi_mem();
	return ret;
}

static void __exit access_tracking_exit(void)
{
	access_ioctl_exit();
	destroy_scan_workqueue();
	if (enable_hist)
		hist_deinit();
	release_adev();
	release_remote_ram();
	reset_acpi_mem();
	memory_notifier_exit();
	pr_info("access tracking exit successfully\n");
}

MODULE_DESCRIPTION("Access driver");
MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
MODULE_LICENSE("GPL v2");
module_init(access_tracking_init);
module_exit(access_tracking_exit);
