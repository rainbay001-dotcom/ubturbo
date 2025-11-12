// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 Tiering Memory Solution: access 内存地址模块
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/mmzone.h>
#include <linux/memory.h>
#include <linux/spinlock.h>

#include "check.h"
#include "access_tracking.h"
#include "access_iomem.h"

#define REMOTE_RAM_NAME "System RAM (Remote)"
#undef pr_fmt
#define pr_fmt(fmt) "access-iomem: " fmt

LIST_HEAD(remote_ram_list);
EXPORT_SYMBOL(remote_ram_list);

DEFINE_RWLOCK(rem_ram_list_lock);
EXPORT_SYMBOL(rem_ram_list_lock);
bool remote_ram_changed; /* protected by rem_ram_list_lock */

int get_remote_ram_len(void)
{
	int len = 0;
	struct ram_segment *seg, *tmp;
	list_for_each_entry_safe(seg, tmp, &remote_ram_list, node) {
		len++;
	}
	return len;
}

static void free_remote_ram(struct list_head *head)
{
	struct ram_segment *seg, *tmp;
	list_for_each_entry_safe(seg, tmp, head, node) {
		list_del(&seg->node);
		kfree(seg);
	}
}

static void move_remote_ram(struct list_head *dst, struct list_head *src)
{
	struct ram_segment *seg, *tmp;
	list_for_each_entry_safe(seg, tmp, src, node) {
		list_move_tail(&seg->node, dst);
	}
}

static void __print_remote_ram(struct list_head *head)
{
	struct ram_segment *seg, *tmp;
	if (list_empty(head)) {
		pr_info("remote ram is empty\n");
		return;
	}
	list_for_each_entry_safe(seg, tmp, head, node) {
		pr_info("[%d] %#llx-%#llx\n", seg->numa_node, seg->start,
			seg->end);
	}
}

static int insert_remote_ram(u64 pa_start, u64 pa_end, struct list_head *head)
{
	struct ram_segment *seg, *tmp;
	u64 start, end;
	unsigned long pfn;
	int nid;

	start = pa_start;
	while (start < pa_end) {
		pfn = PHYS_PFN(start);
		if (!pfn_valid(pfn) || !pfn_to_online_page(pfn)) {
			start += MIN_MEMORY_BLOCK_SIZE;
			continue;
		}
		nid = page_to_nid(pfn_to_online_page(pfn));
		if (nid == NUMA_NO_NODE)
			return -EINVAL;
		end = start + MIN_MEMORY_BLOCK_SIZE - 1;
		if (nid >= SMAP_MAX_NUMNODES) {
			start = end + 1;
			continue;
		}
		seg = kmalloc(sizeof(*seg), GFP_KERNEL);
		if (!seg)
			return -ENOMEM;

		end = MIN(pa_end, end);
		seg->start = start;
		seg->end = end;
		seg->numa_node = nid;

		if (list_empty(head)) {
			list_add_tail(&seg->node, head);
		} else {
			tmp = list_last_entry(head, struct ram_segment, node);
			if (seg->start == tmp->end + 1 &&
			    nid == tmp->numa_node) {
				tmp->end = seg->end;
				kfree(seg);
			} else {
				list_add_tail(&seg->node, head);
			}
		}

		start = end + 1;
	}
	return 0;
}

static int fixed_remote_ram(struct list_head *head)
{
	struct ram_segment *seg = kmalloc(sizeof(*seg), GFP_KERNEL);
	if (!seg)
		return -ENOMEM;
	seg->start = REMOTE_PA_START;
	seg->end = REMOTE_PA_END;
	seg->numa_node = REMOTE_NUMA_ID;
	list_add_tail(&seg->node, head);
	return 0;
}

static int update_resource(struct resource *r, void *arg)
{
	int ret;
	struct list_head *head = (struct list_head *)arg;

	if (!r || !arg)
		return -EINVAL;

	if (r->flags & IORESOURCE_SYSRAM_DRIVER_MANAGED) {
		ret = insert_remote_ram(r->start, r->end, head);
		if (ret) {
			free_remote_ram(head);
			return ret;
		}
	}
	return 0;
}

static int walk_system_ram_remote_range(struct list_head *head)
{
	if (!head)
		return -EINVAL;
	return walk_iomem_res_desc(IORES_DESC_NONE, IORESOURCE_SYSTEM_RAM, 0,
				   -1, head, update_resource);
}

void release_remote_ram(void)
{
	write_lock(&rem_ram_list_lock);
	free_remote_ram(&remote_ram_list);
	write_unlock(&rem_ram_list_lock);
}

bool ram_changed(void)
{
	bool flag;

	if (smap_scene == UB_QEMU_SCENE)
		return false;

	read_lock(&rem_ram_list_lock);
	flag = remote_ram_changed;
	read_unlock(&rem_ram_list_lock);
	return flag;
}
EXPORT_SYMBOL(ram_changed);

int refresh_remote_ram(void)
{
	LIST_HEAD(tmp_head);
	int ret;
	if (smap_scene == UB_QEMU_SCENE)
		ret = fixed_remote_ram(&tmp_head);
	else
		ret = walk_system_ram_remote_range(&tmp_head);
	if (ret)
		return ret;
	write_lock(&rem_ram_list_lock);
	free_remote_ram(&remote_ram_list);
	move_remote_ram(&remote_ram_list, &tmp_head);
	free_remote_ram(&tmp_head);
	__print_remote_ram(&remote_ram_list);
	remote_ram_changed = true;
	pr_info("set ram changed to %d\n", remote_ram_changed);
	write_unlock(&rem_ram_list_lock);
	return 0;
}

int get_numa_by_pfn(unsigned long pfn)
{
	struct ram_segment *seg;
	u64 pa = PFN_PHYS(pfn);
	int nid = NUMA_NO_NODE;

	read_lock(&rem_ram_list_lock);
	list_for_each_entry(seg, &remote_ram_list, node) {
		if (pa >= seg->start && pa <= seg->end) {
			nid = seg->numa_node;
			break;
		}
	}
	read_unlock(&rem_ram_list_lock);
	return nid;
}

u64 get_node_page_cnt_iomem(int nid, int page_size)
{
	u64 len = 0;
	struct ram_segment *seg, *tmp;

	if (unlikely(nid < nr_local_numa))
		return 0;

	read_lock(&rem_ram_list_lock);
	list_for_each_entry_safe(seg, tmp, &remote_ram_list, node) {
		if (seg->numa_node != nid)
			continue;

		if (page_size == PAGE_SIZE_2M)
			len += calc_2m_count(seg->end - seg->start + 1);
		else if (page_size == PAGE_SIZE_4K)
			len += calc_4k_count(seg->end - seg->start + 1);
	}
	read_unlock(&rem_ram_list_lock);

	return len;
}
EXPORT_SYMBOL(get_node_page_cnt_iomem);

int calc_paddr_acidx_iomem(u64 pa, int *nid, u64 *index, int page_size)
{
	struct ram_segment *seg;
	int shift = __builtin_ctz(page_size);
	int numa = NUMA_NO_NODE;
	u64 idx[SMAP_MAX_REMOTE_NUMNODES] = { 0 };

	read_lock(&rem_ram_list_lock);
	list_for_each_entry(seg, &remote_ram_list, node) {
		if (pa < seg->start)
			break;
		if (pa <= seg->end) {
			numa = seg->numa_node;
			idx[numa] += ((pa - seg->start) >> shift);
			break;
		}
		idx[seg->numa_node] += ((seg->end - seg->start + 1) >> shift);
	}

	if (unlikely(list_entry_is_head(seg, &remote_ram_list, node))) {
		read_unlock(&rem_ram_list_lock);
		return -ERANGE;
	}
	read_unlock(&rem_ram_list_lock);
	if (unlikely(numa == NUMA_NO_NODE))
		return -ERANGE;
	*nid = numa;
	*index = idx[numa];
	return 0;
}

int calc_acidx_paddr_iomem(int nid, u64 acidx, u64 *paddr, int page_size)
{
	struct ram_segment *seg;
	u64 range;
	int shift = __builtin_ctz(page_size);
	u64 offset = acidx << shift;

	read_lock(&rem_ram_list_lock);
	list_for_each_entry(seg, &remote_ram_list, node) {
		if (seg->numa_node != nid)
			continue;
		range = seg->end - seg->start + 1;
		if (offset >= range) {
			offset -= range;
			continue;
		}
		*paddr = seg->start + offset;
		read_unlock(&rem_ram_list_lock);
		return 0;
	}
	read_unlock(&rem_ram_list_lock);
	return -ERANGE;
}

/* check if [pa_start, pa_end] belongs to numa */
int smap_is_remote_addr_valid(int nid, u64 pa_start, u64 pa_end)
{
	struct ram_segment *seg = NULL;

	if (pa_start >= pa_end) {
		pr_err("pa_start(%#llx) >= pa_end(%#llx)\n", pa_start, pa_end);
		return -EINVAL;
	}

	read_lock(&rem_ram_list_lock);
	list_for_each_entry(seg, &remote_ram_list, node) {
		if (seg->numa_node != nid)
			continue;
		if (pa_start >= seg->start && pa_end <= seg->end + 1) {
			read_unlock(&rem_ram_list_lock);
			return 0;
		}
	}
	read_unlock(&rem_ram_list_lock);
	return -EINVAL;
}
EXPORT_SYMBOL(smap_is_remote_addr_valid);
