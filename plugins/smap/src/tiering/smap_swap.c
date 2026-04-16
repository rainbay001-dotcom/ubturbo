// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: SMAP NVMe Swap - Core swap-out and swap-in logic
 *
 * This module implements the "Option A" NVMe swap path.  Pages identified
 * as cold on L2 NUMA nodes are written to an NVMe device through a custom
 * bio path (bypassing the kernel's generic swap subsystem).
 *
 * Instead of directly manipulating PTEs (which would require arch-specific
 * code and tight coupling to mm internals), we use a kernel-side hash table
 * to record the mapping between NVMe slot indices and (pid, vaddr) pairs.
 * The actual page un-mapping is done via try_to_unmap (rmap) when possible,
 * or deferred to user-space via the existing madvise path.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/pagemap.h>
#include <linux/page-flags.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/hashtable.h>

#include "common.h"
#include "smap_swap.h"

#undef pr_fmt
#define pr_fmt(fmt) "SMAP_swap: " fmt

/* Global swap device state */
static struct smap_swap_device g_swap_dev;
static bool g_swap_initialized;
static DEFINE_MUTEX(g_swap_mutex);

/*
 * ============================================================
 * Mapping table helpers
 * ============================================================
 */

static void mapping_add(u32 slot_index, pid_t pid, u64 vaddr)
{
	struct smap_swap_mapping *entry;
	unsigned long flags;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		pr_err("failed to allocate mapping entry for slot %u\n",
		       slot_index);
		return;
	}

	entry->slot_index = slot_index;
	entry->pid = pid;
	entry->vaddr = vaddr;

	spin_lock_irqsave(&g_swap_dev.map_lock, flags);
	hash_add(g_swap_dev.mappings, &entry->node, slot_index);
	spin_unlock_irqrestore(&g_swap_dev.map_lock, flags);
}

static struct smap_swap_mapping *mapping_find(u32 slot_index)
{
	struct smap_swap_mapping *entry;
	unsigned long flags;

	spin_lock_irqsave(&g_swap_dev.map_lock, flags);
	hash_for_each_possible(g_swap_dev.mappings, entry, node, slot_index) {
		if (entry->slot_index == slot_index) {
			spin_unlock_irqrestore(&g_swap_dev.map_lock, flags);
			return entry;
		}
	}
	spin_unlock_irqrestore(&g_swap_dev.map_lock, flags);
	return NULL;
}

static void mapping_remove(u32 slot_index)
{
	struct smap_swap_mapping *entry;
	unsigned long flags;

	spin_lock_irqsave(&g_swap_dev.map_lock, flags);
	hash_for_each_possible(g_swap_dev.mappings, entry, node, slot_index) {
		if (entry->slot_index == slot_index) {
			hash_del(&entry->node);
			spin_unlock_irqrestore(&g_swap_dev.map_lock, flags);
			kfree(entry);
			return;
		}
	}
	spin_unlock_irqrestore(&g_swap_dev.map_lock, flags);
}

static void mapping_destroy_all(void)
{
	struct smap_swap_mapping *entry;
	struct hlist_node *tmp;
	unsigned long flags;
	int bkt;

	spin_lock_irqsave(&g_swap_dev.map_lock, flags);
	hash_for_each_safe(g_swap_dev.mappings, bkt, tmp, entry, node) {
		hash_del(&entry->node);
		kfree(entry);
	}
	spin_unlock_irqrestore(&g_swap_dev.map_lock, flags);
}

/*
 * ============================================================
 * Initialisation / teardown
 * ============================================================
 */

/**
 * smap_swap_init - Initialize the NVMe swap subsystem
 * @config: Device configuration from user-space
 *
 * Opens the block device, creates 4K (and optionally 2M) bitmap
 * allocators, and marks the subsystem ready.
 */
int smap_swap_init(struct swap_dev_config *config)
{
	int ret;
	u64 dev_size;

	mutex_lock(&g_swap_mutex);

	if (g_swap_initialized) {
		pr_warn("swap already initialized, ignoring\n");
		mutex_unlock(&g_swap_mutex);
		return -EBUSY;
	}

	memset(&g_swap_dev, 0, sizeof(g_swap_dev));
	spin_lock_init(&g_swap_dev.lock);
	spin_lock_init(&g_swap_dev.map_lock);
	hash_init(g_swap_dev.mappings);
	atomic64_set(&g_swap_dev.total_swap_out, 0);
	atomic64_set(&g_swap_dev.total_swap_in, 0);
	atomic64_set(&g_swap_dev.current_swapped, 0);

	ret = smap_swap_bio_init(&g_swap_dev, config->path);
	if (ret) {
		pr_err("failed to open block device %s: %d\n",
		       config->path, ret);
		mutex_unlock(&g_swap_mutex);
		return ret;
	}

	dev_size = config->size ? config->size : g_swap_dev.capacity;
	g_swap_dev.capacity = dev_size;

	/* Create 4K slot map */
	g_swap_dev.map_4k = smap_swap_map_create(dev_size, SWAP_SLOT_4K);
	if (IS_ERR(g_swap_dev.map_4k)) {
		ret = PTR_ERR(g_swap_dev.map_4k);
		g_swap_dev.map_4k = NULL;
		pr_err("failed to create 4K swap map: %d\n", ret);
		goto err_bio;
	}

	/* Optionally create 2M slot map (uses separate address space) */
	if (config->enable_2m) {
		g_swap_dev.map_2m = smap_swap_map_create(dev_size,
							  SWAP_SLOT_2M);
		if (IS_ERR(g_swap_dev.map_2m)) {
			ret = PTR_ERR(g_swap_dev.map_2m);
			g_swap_dev.map_2m = NULL;
			pr_warn("failed to create 2M swap map: %d (continuing without)\n",
				ret);
			/* Non-fatal: we can still use 4K slots */
		}
	}

	g_swap_initialized = true;
	mutex_unlock(&g_swap_mutex);

	pr_info("NVMe swap initialized: capacity=%llu MB, 4K_slots=%u\n",
		dev_size >> 20,
		g_swap_dev.map_4k->total_slots);

	return 0;

err_bio:
	smap_swap_bio_exit(&g_swap_dev);
	mutex_unlock(&g_swap_mutex);
	return ret;
}

/**
 * smap_swap_exit - Tear down the NVMe swap subsystem
 */
void smap_swap_exit(void)
{
	mutex_lock(&g_swap_mutex);

	if (!g_swap_initialized) {
		mutex_unlock(&g_swap_mutex);
		return;
	}

	g_swap_initialized = false;

	mapping_destroy_all();

	smap_swap_map_destroy(g_swap_dev.map_4k);
	g_swap_dev.map_4k = NULL;

	smap_swap_map_destroy(g_swap_dev.map_2m);
	g_swap_dev.map_2m = NULL;

	smap_swap_bio_exit(&g_swap_dev);

	pr_info("NVMe swap shutdown complete: swapped_out=%lld swapped_in=%lld\n",
		atomic64_read(&g_swap_dev.total_swap_out),
		atomic64_read(&g_swap_dev.total_swap_in));

	mutex_unlock(&g_swap_mutex);
}

/*
 * ============================================================
 * Swap-out path
 * ============================================================
 */

/**
 * do_swap_out - Swap cold pages from RAM to NVMe
 * @umsg: User-space swap_msg pointer
 *
 * For each physical address in the message:
 *   1. Convert PFN to struct page
 *   2. Allocate an NVMe slot
 *   3. Write the page synchronously
 *   4. Record the (slot -> pid, vaddr) mapping
 *   5. Store the slot index in results
 *
 * Returns 0 on success, negative errno on failure.
 */
int do_swap_out(struct swap_msg __user *umsg)
{
	struct swap_msg msg;
	u64 *addrs = NULL;
	u64 *results = NULL;
	int ret = 0;
	int i;
	int swapped = 0;

	if (!g_swap_initialized) {
		pr_err("swap not initialized\n");
		return -ENODEV;
	}

	if (copy_from_user(&msg, umsg, sizeof(msg)))
		return -EFAULT;

	if (msg.cnt <= 0 || msg.cnt > MAX_SWAP_ADDRS) {
		pr_err("invalid swap-out count: %d\n", msg.cnt);
		return -EINVAL;
	}

	addrs = kvzalloc(msg.cnt * sizeof(u64), GFP_KERNEL);
	results = kvzalloc(msg.cnt * sizeof(u64), GFP_KERNEL);
	if (!addrs || !results) {
		ret = -ENOMEM;
		goto out_free;
	}

	if (copy_from_user(addrs, msg.addrs, msg.cnt * sizeof(u64))) {
		ret = -EFAULT;
		goto out_free;
	}

	for (i = 0; i < msg.cnt; i++) {
		u64 phys_addr = addrs[i];
		unsigned long pfn = phys_addr >> PAGE_SHIFT;
		struct page *page;
		int slot;
		int write_ret;

		/* Validate PFN */
		if (!pfn_valid(pfn)) {
			pr_debug("invalid pfn %lu for addr 0x%llx\n",
				 pfn, phys_addr);
			results[i] = (u64)-1;
			continue;
		}

		page = pfn_to_page(pfn);

		/*
		 * Basic sanity checks on the page.  We skip pages that are
		 * reserved, part of the slab, or otherwise not suitable for
		 * swap-out.
		 */
		if (PageReserved(page) || PageSlab(page) ||
		    page_count(page) == 0) {
			pr_debug("skipping non-swappable page pfn=%lu flags=0x%lx\n",
				 pfn, page->flags);
			results[i] = (u64)-1;
			continue;
		}

		/* Allocate a swap slot */
		slot = smap_swap_map_alloc(g_swap_dev.map_4k);
		if (slot < 0) {
			pr_warn("swap map full, stopping at page %d/%d\n",
				i, msg.cnt);
			results[i] = (u64)-1;
			ret = -ENOSPC;
			break;
		}

		/* Get a reference to the page for the duration of the write */
		get_page(page);

		/* Write page content to NVMe */
		write_ret = smap_swap_write_page_sync(&g_swap_dev, page,
						      (u32)slot);
		put_page(page);

		if (write_ret) {
			pr_err("write failed for slot %d, pfn %lu: %d\n",
			       slot, pfn, write_ret);
			smap_swap_map_free(g_swap_dev.map_4k, (u32)slot);
			results[i] = (u64)-1;
			continue;
		}

		/*
		 * Record the mapping.  We store the PID and the physical
		 * address as the "vaddr" field.  The user-space daemon knows
		 * the VA from its pagemap walk; here we just need enough
		 * information to find the page on swap-in.
		 */
		mapping_add((u32)slot, msg.pid, phys_addr);
		results[i] = (u64)slot;
		swapped++;

		atomic64_inc(&g_swap_dev.total_swap_out);
		atomic64_inc(&g_swap_dev.current_swapped);
	}

	/* Copy results back to user space */
	if (copy_to_user(msg.results, results, msg.cnt * sizeof(u64))) {
		ret = -EFAULT;
		goto out_free;
	}

	pr_info("swap-out: pid=%d requested=%d swapped=%d\n",
		msg.pid, msg.cnt, swapped);

out_free:
	kvfree(addrs);
	kvfree(results);
	return ret;
}

/*
 * ============================================================
 * Swap-in path
 * ============================================================
 */

/**
 * do_swap_in - Read pages back from NVMe into RAM
 * @umsg: User-space swap_in_msg pointer
 *
 * For each slot index:
 *   1. Look up the mapping to find original (pid, phys_addr)
 *   2. Allocate a new page (optionally on a specific NUMA node)
 *   3. Read from NVMe into the new page
 *   4. Free the swap slot and remove mapping
 *   5. Store the new page's PFN in results
 *
 * Returns 0 on success, negative errno on failure.
 */
int do_swap_in(struct swap_in_msg __user *umsg)
{
	struct swap_in_msg msg;
	u64 *slots = NULL;
	u64 *results = NULL;
	int ret = 0;
	int i;
	int restored = 0;

	if (!g_swap_initialized) {
		pr_err("swap not initialized\n");
		return -ENODEV;
	}

	if (copy_from_user(&msg, umsg, sizeof(msg)))
		return -EFAULT;

	if (msg.cnt <= 0 || msg.cnt > MAX_SWAP_ADDRS) {
		pr_err("invalid swap-in count: %d\n", msg.cnt);
		return -EINVAL;
	}

	slots = kvzalloc(msg.cnt * sizeof(u64), GFP_KERNEL);
	results = kvzalloc(msg.cnt * sizeof(u64), GFP_KERNEL);
	if (!slots || !results) {
		ret = -ENOMEM;
		goto out_free;
	}

	if (copy_from_user(slots, msg.slots, msg.cnt * sizeof(u64))) {
		ret = -EFAULT;
		goto out_free;
	}

	for (i = 0; i < msg.cnt; i++) {
		u32 slot_index = (u32)slots[i];
		struct smap_swap_mapping *entry;
		struct page *new_page;
		int read_ret;

		entry = mapping_find(slot_index);
		if (!entry) {
			pr_debug("no mapping for slot %u\n", slot_index);
			results[i] = (u64)-1;
			continue;
		}

		/* Allocate a new page, optionally on a specific node */
		if (msg.dest_nid >= 0 && msg.dest_nid < MAX_NUMNODES &&
		    node_online(msg.dest_nid)) {
			new_page = alloc_pages_node(msg.dest_nid,
						    GFP_HIGHUSER_MOVABLE, 0);
		} else {
			new_page = alloc_pages(GFP_HIGHUSER_MOVABLE, 0);
		}

		if (!new_page) {
			pr_err("failed to allocate page for swap-in slot %u\n",
			       slot_index);
			results[i] = (u64)-1;
			continue;
		}

		/* Read page content from NVMe */
		read_ret = smap_swap_read_page_sync(&g_swap_dev, new_page,
						    slot_index);
		if (read_ret) {
			pr_err("read failed for slot %u: %d\n",
			       slot_index, read_ret);
			__free_page(new_page);
			results[i] = (u64)-1;
			continue;
		}

		/* Return the new page's PFN to user-space */
		results[i] = page_to_pfn(new_page) << PAGE_SHIFT;

		/* Free the swap slot and remove mapping */
		smap_swap_map_free(g_swap_dev.map_4k, slot_index);
		mapping_remove(slot_index);
		restored++;

		atomic64_inc(&g_swap_dev.total_swap_in);
		atomic64_dec(&g_swap_dev.current_swapped);
	}

	if (copy_to_user(msg.results, results, msg.cnt * sizeof(u64))) {
		ret = -EFAULT;
		goto out_free;
	}

	pr_info("swap-in: pid=%d requested=%d restored=%d dest_nid=%d\n",
		msg.pid, msg.cnt, restored, msg.dest_nid);

out_free:
	kvfree(slots);
	kvfree(results);
	return ret;
}

/*
 * ============================================================
 * Device configuration and statistics
 * ============================================================
 */

/**
 * smap_swap_set_device - Configure the NVMe device via ioctl
 * @uconfig: User-space swap_dev_config pointer
 */
int smap_swap_set_device(struct swap_dev_config __user *uconfig)
{
	struct swap_dev_config config;

	if (copy_from_user(&config, uconfig, sizeof(config)))
		return -EFAULT;

	config.path[sizeof(config.path) - 1] = '\0';

	if (config.path[0] == '\0') {
		pr_err("empty device path\n");
		return -EINVAL;
	}

	pr_info("configuring swap device: path=%s size=%llu enable_2m=%d\n",
		config.path, config.size, config.enable_2m);

	/* If already initialized, tear down first */
	if (g_swap_initialized)
		smap_swap_exit();

	return smap_swap_init(&config);
}

/**
 * smap_swap_get_stats - Return swap statistics to user-space
 * @ustats: User-space swap_stats pointer
 */
int smap_swap_get_stats(struct swap_stats __user *ustats)
{
	struct swap_stats stats;
	u32 total_4k = 0, free_4k = 0;
	u32 total_2m = 0, free_2m = 0;

	memset(&stats, 0, sizeof(stats));

	if (!g_swap_initialized) {
		if (copy_to_user(ustats, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	}

	stats.total_swap_out = atomic64_read(&g_swap_dev.total_swap_out);
	stats.total_swap_in = atomic64_read(&g_swap_dev.total_swap_in);
	stats.current_swapped = atomic64_read(&g_swap_dev.current_swapped);

	smap_swap_map_get_stats(g_swap_dev.map_4k, &total_4k, &free_4k);
	stats.free_slots_4k = free_4k;

	if (g_swap_dev.map_2m) {
		smap_swap_map_get_stats(g_swap_dev.map_2m, &total_2m,
					&free_2m);
		stats.free_slots_2m = free_2m;
	}

	if (copy_to_user(ustats, &stats, sizeof(stats)))
		return -EFAULT;

	return 0;
}

/*
 * ============================================================
 * Module-level init/exit (called from mig_init.c)
 * ============================================================
 */

/**
 * smap_swap_module_init - One-time module initialisation
 *
 * The actual device setup is deferred until user-space calls
 * SMAP_SWAP_DEV_SET ioctl.
 */
int smap_swap_module_init(void)
{
	g_swap_initialized = false;
	mutex_init(&g_swap_mutex);
	pr_info("NVMe swap module loaded (deferred init)\n");
	return 0;
}

/**
 * smap_swap_module_exit - Module teardown
 */
void smap_swap_module_exit(void)
{
	if (g_swap_initialized)
		smap_swap_exit();
	pr_info("NVMe swap module unloaded\n");
}
