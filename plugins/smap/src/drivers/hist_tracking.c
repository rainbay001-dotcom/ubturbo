// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP : hist_dev
 */
#include <asm/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/vmalloc.h>
#include <linux/mmzone.h>
#include <linux/pfn.h>
#include <linux/workqueue.h>
#include <linux/hugetlb.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/spinlock.h>

#include "check.h"
#include "access_iomem.h"
#include "access_acpi_mem.h"
#include "access_tracking.h"
#include "bus.h"
#include "ub_hist.h"
#include "hist_ops.h"
#include "hist_tracking.h"

#define to_access_tracking_dev(n) container_of(n, struct access_tracking_dev, ldev)
#define to_delay_work(n) container_of(n, struct delayed_work, work)
#define delay_work_to_dev(n) \
	container_of(n, struct access_tracking_dev, scan_work)
#define WORK_QUEUE_NAME_LEN 32
#define HIST_TRACKING_DEFAULT_PERIOD 200
#define SCAN_TIME_MAX 100000

#undef pr_fmt
#define pr_fmt(fmt) "hist: " fmt

extern struct list_head access_dev;

static inline void reset_actc_data(struct access_tracking_dev *hdev)
{
	size_t len = hdev->page_count * sizeof(u16);

	if (hdev->access_bit_actc_data)
		memset(hdev->access_bit_actc_data, 0, len);
}

static void hist_tracking_enable(struct device *ldev)
{
	struct access_tracking_dev *hdev;

	hdev = to_access_tracking_dev(ldev);
	down_write(&hdev->buffer_lock);
	reset_actc_data(hdev);
	up_write(&hdev->buffer_lock);
	hdev->enable_on = true;
	hist_thread_resume();
}

static void hist_tracking_disable(struct device *ldev)
{
	struct access_tracking_dev *hdev;

	hdev = to_access_tracking_dev(ldev);
	hdev->enable_on = false;
	hist_thread_pause();
}

static void actc_buffer_deinit(struct access_tracking_dev *hdev)
{
	hdev->page_count = 0;
	if (hdev->access_bit_actc_data) {
		vfree(hdev->access_bit_actc_data);
		hdev->access_bit_actc_data = NULL;
	}
}

static inline int hist_get_page_size(struct access_tracking_dev *hdev)
{
	if (hdev->page_size_mode == PAGE_MODE_2M) {
		return g_pagesize_huge;
	}
	return PAGE_SIZE;
}

static u64 calc_access_len(struct access_tracking_dev *hdev)
{
	int page_size = hist_get_page_size(hdev);
	u64 page_count;
	if (hdev->node >= nr_local_numa) {
		page_count = get_node_page_cnt_iomem(hdev->node, page_size);
	} else {
		page_count = get_node_actc_len(hdev->node, page_size);
	}
	pr_debug("histogram tracking node: %d, got page count: %llu\n",
		 hdev->node, page_count);

	return page_count;
}

static void hist_dev_pgsize_update(u8 page_size_mode)
{
	u32 pgsize = page_size_mode == PAGE_MODE_2M ? SIZE_2M : SIZE_4K;
	hist_update_pgsize(pgsize);
}

static int actc_buffer_reinit(struct access_tracking_dev *hdev)
{
	u64 page_count;
	page_count = calc_access_len(hdev);
	if (!page_count) {
		pr_info("no page found on node: %d\n", hdev->node);
	}
	if (hdev->page_count == page_count) {
		reset_actc_data(hdev);
		return 0;
	}
	actc_buffer_deinit(hdev);
	if (page_count) {
		hdev->access_bit_actc_data = vzalloc(page_count * sizeof(u16));
		if (!hdev->access_bit_actc_data) {
			return -ENOMEM;
		}
	}
	hdev->page_count = page_count;
	return 0;
}

static int hist_tracking_reinit_actc_buffer(struct device *ldev)
{
	int ret;
	struct access_tracking_dev *hdev = to_access_tracking_dev(ldev);
	hist_set_iomem();
	down_write(&hdev->buffer_lock);
	ret = actc_buffer_reinit(hdev);
	if (ret) {
		pr_err("Actc buffer reinit failed. ret:%d\n", ret);
	}
	up_write(&hdev->buffer_lock);
	return ret;
}

static int hist_tracking_set_page_size(struct device *ldev, u8 pgsize)
{
	int ret;
	struct access_tracking_dev *hdev;
	hdev = to_access_tracking_dev(ldev);

	if (pgsize != PAGE_MODE_4K && pgsize != PAGE_MODE_2M) {
		pr_err("invalid page size\n");
		return -EINVAL;
	}
	down_write(&hdev->buffer_lock);
	hdev->enable_on = false;
	hdev->page_size_mode = pgsize;
	hist_dev_pgsize_update(pgsize);
	ret = actc_buffer_reinit(hdev);
	if (ret) {
		up_write(&hdev->buffer_lock);
		pr_err("Actc buffer reinit failed. ret:%d\n", ret);
		return ret;
	}
	up_write(&hdev->buffer_lock);
	return ret;
}

static struct tracking_operations g_hist_tracking_ops = {
	.tracking_enable = hist_tracking_enable,
	.tracking_disable = hist_tracking_disable,
	.tracking_reinit_actc_buffer = hist_tracking_reinit_actc_buffer,
	.tracking_set_page_size = hist_tracking_set_page_size,
};

static int actc_buffer_init(struct access_tracking_dev *hdev)
{
	hdev->page_count = calc_access_len(hdev);
	pr_info("page count: %llu for node: %d\n", hdev->page_count,
		hdev->node);
	if (!hdev->page_count)
		return 0;
	hdev->access_bit_actc_data = vzalloc(hdev->page_count * sizeof(u16));
	if (!hdev->access_bit_actc_data) {
		pr_err("unable to alloc mem for histogram tracking ACTC buffer\n");
		hdev->page_count = 0;
		return -ENOMEM;
	}
	return 0;
}

static void scan_hist(struct access_tracking_dev *hdev)
{
	u32 pgcount = 0, addr_pg = 0;
	u64 dev_page_count = hdev->page_count;
	struct ram_segment *rseg, *tmp;
	struct addr_seg addr_seg;
	if (PAGE_SIZE == PAGE_SIZE_64K)
		dev_page_count *= PAGE_SIZE_64K_DIV_4K;
	read_lock(&rem_ram_list_lock);
	list_for_each_entry_safe(rseg, tmp, &remote_ram_list, node) {
		if (rseg->numa_node != hdev->node) {
			continue;
		}
		addr_seg.start = rseg->start;
		addr_seg.size = rseg->end - rseg->start + 1;
		addr_pg = addr_seg.size >>
			  (hdev->page_size_mode == PAGE_MODE_2M ?
				   HIST_ADDR_SHIFT_2M :
				   HIST_ADDR_SHIFT_4K);
		if (pgcount + addr_pg > dev_page_count) {
			pr_warn("page index: %u exceeds upper bound of page count: %llu\n",
				pgcount + addr_pg, dev_page_count);
			break;
		}
		fetch_hist_actc_buf(hdev->access_bit_actc_data + pgcount, &addr_seg);
		pgcount += addr_pg;
	}
	read_unlock(&rem_ram_list_lock);
}

static void update_hist_actc_batch(void)
{
	struct access_tracking_dev *hdev, *n;
	list_for_each_entry_safe(hdev, n, &access_dev, list) {
		if (!hdev->is_hist)
			continue;
		down_write(&hdev->buffer_lock);
		scan_hist(hdev);
		up_write(&hdev->buffer_lock);
	}
}

static void hist_tracking_deinit(void)
{
	struct access_tracking_dev *hdev, *n;
	list_for_each_entry_safe(hdev, n, &access_dev, list) {
		if (!hdev->is_hist)
			continue;
		tracking_dev_remove(hdev->tracking_dev);
		actc_buffer_deinit(hdev);
		device_unregister(&hdev->ldev);
		kfree(hdev);
	}
}

void access_tracking_dev_release(struct device *dev)
{
	pr_debug("Releasing device %s\n", dev_name(dev));
}

static int hist_tracking_init(void)
{
	int ret;
	unsigned int node;
	struct access_tracking_dev *hdev;

	if (nr_local_numa < 0)
		return -EINVAL;

	for (node = (unsigned int)nr_local_numa; node < SMAP_MAX_NUMNODES;
	     ++node) {
		hdev = kzalloc(sizeof(struct access_tracking_dev), GFP_KERNEL);
		if (!hdev) {
			pr_err("unable to alloc mem for histogram tracking device\n");
			goto put_dev;
		}

		hdev->node = node;
		hdev->page_size_mode = PAGE_MODE_2M;
		hdev->is_hist = true;

		ret = actc_buffer_init(hdev);
		if (ret) {
			pr_err("unable to init ACTC buffer, ret: %d\n", ret);
			goto free_hdev;
		}

		init_rwsem(&hdev->buffer_lock);
		device_initialize(&hdev->ldev);
		hdev->ldev.release = access_tracking_dev_release;
		ret = dev_set_name(&hdev->ldev, "hist_tracking_dev%d", node);
		if (ret) {
			pr_err("unable to set histogram tracking device name, ret: %d\n",
			       ret);
			goto deinit_buf;
		}

		ret = device_add(&hdev->ldev);
		if (ret) {
			pr_err("unable to add histogram tracking device\n");
			goto deinit_buf;
		}

		hdev->tracking_dev = tracking_dev_add(
			&hdev->ldev, &g_hist_tracking_ops, hdev->node);
		if (!hdev->tracking_dev) {
			pr_err("unable to add tracking for node: %d\n",
			       hdev->node);
			goto del_dev;
		}

		list_add_tail(&hdev->list, &access_dev);
	}

	return 0;

del_dev:
	device_del(&hdev->ldev);
deinit_buf:
	actc_buffer_deinit(hdev);
	put_device(&hdev->ldev);
free_hdev:
	kfree(hdev);
put_dev:
	hist_tracking_deinit();
	pr_err("smap tracking dev init failed.\n");
	return -ENODEV;
}

int hist_module_init(void)
{
	int ret;

	ret = hist_init(SIZE_2M, smap_scene != NORMAL_SCENE);
	if (ret) {
		pr_err("init SMAP histogram device failed, ret: %d\n", ret);
		return ret;
	}

	ret = hist_tracking_init();
	if (ret) {
		pr_err("init histogram tracking device failed, ret: %d\n", ret);
		goto err_tracking_add;
	}
	hist_set_flush_actc_cb(update_hist_actc_batch);
	pr_info("smap hist tracking init success.\n");
	return 0;

err_tracking_add:
	hist_deinit();
	return ret;
}
