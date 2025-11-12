// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: smap memory notifier
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/memory.h>

#include "bus.h"
#include "check.h"
#include "access_acpi_mem.h"
#include "access_iomem.h"
#include "access_tracking.h"
#include "memory_notifier.h"

#undef pr_fmt
#define pr_fmt(fmt) "smap memory notifier: " fmt

#define SMAP_MN_CALLBACK_PRI 100

static int memory_notifier_cb(struct memory_notify *mnb, unsigned long action)
{
	int ret;
	int nid;
	struct page *page;
	unsigned long start_pfn = mnb->start_pfn;

	pr_info("detected pfn range %s\n",
		(action == MEM_ONLINE ? "online" : "offline"));

	if (smap_scene == UB_QEMU_SCENE)
		return 0;

	/* find the node to which the memory block belongs */
	if (action == MEM_ONLINE) {
		if (!pfn_valid(start_pfn)) {
			pr_err("start_pfn %#lx is invalid\n", start_pfn);
			return -EINVAL;
		}
		page = pfn_to_online_page(start_pfn);
		if (!page) {
			pr_err("find pfn %#lx page failed\n", start_pfn);
			return -EINVAL;
		}
		nid = page_to_nid(page);
	} else {
		nid = get_numa_by_pfn(start_pfn);
	}
	if (nid == NUMA_NO_NODE) {
		pr_warn("search node of the pfn %#lx failed\n", start_pfn);
	}
	pr_info("pfn %#lx belongs to node%d\n", start_pfn, nid);

	ret = refresh_remote_ram();
	if (ret) {
		pr_err("update remote ram err: %d\n", ret);
		return ret;
	}
	pr_info("update remote ram node%d done\n", nid);

	ret = tracking_core_reinit_actc_buffer(nid);
	if (ret)
		pr_err("reinit node%d buffer ret: %d\n", nid, ret);

	return ret;
}

static int pre_offline_notifier_cb(struct memory_notify *mnb,
				   unsigned long action)
{
	unsigned long start_pfn = mnb->start_pfn;
	unsigned long end_pfn = mnb->start_pfn + mnb->nr_pages - 1;

	pr_info("detected pfn range %#lx-%#lx going offline\n", start_pfn,
		end_pfn);

	if (smap_scene == UB_QEMU_SCENE)
		return 0;

	return 0;
}

static int smap_memory_notifier(struct notifier_block *self,
				unsigned long action, void *arg)
{
	struct memory_notify *mnb = (struct memory_notify *)arg;
	int ret = 0;

	switch (action) {
	case MEM_ONLINE:
	case MEM_OFFLINE:
		ret = memory_notifier_cb(mnb, action);
		break;
	case MEM_GOING_OFFLINE:
		ret = pre_offline_notifier_cb(mnb, action);
		break;
	default:
		break;
	}

	pr_info("handle memory notify ret: %d\n", ret);
	return NOTIFY_OK;
}

static struct notifier_block smap_nb = {
	.notifier_call = smap_memory_notifier,
	.priority = SMAP_MN_CALLBACK_PRI,
};

int memory_notifier_init(void)
{
	int ret = register_memory_notifier(&smap_nb);
	pr_info("register memory notifier: %d\n", ret);

	return ret;
}

void memory_notifier_exit(void)
{
	unregister_memory_notifier(&smap_nb);
	pr_info("unregister memory notifier\n");
}
