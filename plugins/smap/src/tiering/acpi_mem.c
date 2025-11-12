// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 acpi memory module
 */

#include <linux/types.h>
#include <linux/hugetlb.h>
#include "smap_msg.h"
#include "smap_migrate_wrapper.h"
#include "acpi_helper.h"
#include "acpi_mem.h"

#undef pr_fmt
#define pr_fmt(fmt) "smap_acpi: " fmt

int nr_local_numa = 0;
struct mem_info acpi_mem = { .mem = LIST_HEAD_INIT(acpi_mem.mem) };
static struct node_mem acpi_mem_cached[SMAP_MAX_NUMNODES];

bool is_paddr_local(u64 pa)
{
	struct acpi_mem_segment *mem;
	list_for_each_entry(mem, &acpi_mem.mem, segment) {
		if (mem->start <= pa && pa <= mem->end)
			return true;
	}
	return false;
}

static int acpi_table_build_mem(struct acpi_subtable_header *header)
{
	int node;
	u32 hotpluggable;
	struct acpi_mem_segment *mem, *tmp;
	struct acpi_srat_mem_affinity *p =
		(struct acpi_srat_mem_affinity *)header;

	if ((p->flags & ACPI_SRAT_MEM_ENABLED) == 0) {
		return 0;
	}
	hotpluggable = p->flags & ACPI_SRAT_MEM_HOT_PLUGGABLE;
	if (hotpluggable && !IS_ENABLED(CONFIG_MEMORY_HOTPLUG)) {
		return 0;
	}

	mem = kmalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		return -ENOMEM;
	}

	mem->start = p->base_address;
	mem->end = mem->start + p->length - 1;
	mem->pxm = p->proximity_domain;
	node = pxm_to_node(mem->pxm);
	if (node == NUMA_NO_NODE) {
		pr_err("pxm to node failed, ret is %d\n", node);
		return -EINVAL;
	}
	mem->node = node;
	/* add to list and ensure increment order of acpi_mem.mem */
	if (list_empty(&acpi_mem.mem)) {
		list_add_tail(&mem->segment, &acpi_mem.mem);
	} else {
		list_for_each_entry_reverse(tmp, &acpi_mem.mem, segment) {
			if (tmp->start < mem->start)
				break;
		}
		if (list_entry_is_head(tmp, &acpi_mem.mem, segment))
			list_add(&mem->segment, &acpi_mem.mem);
		else
			list_add(&mem->segment, &tmp->segment);
	}
	acpi_mem.len++;

	return 0;
}

static int acpi_parse_memory_affinity(union acpi_subtable_headers *header,
				      const unsigned long end)
{
	struct acpi_srat_mem_affinity *memory_affinity;

	memory_affinity = (struct acpi_srat_mem_affinity *)header;
	return acpi_table_build_mem(&header->common);
}

static void calc_node_distance(void)
{
	int i;
	int nid;

	for (i = 0; i < ARRAY_SIZE(acpi_mem_cached); i++) {
		if (!acpi_mem_cached[i].online) {
			continue;
		}
		for_each_node_state(nid, N_NORMAL_MEMORY) {
			if (is_node_invalid(nid))
				continue;
			acpi_mem_cached[i].distance[nid] =
				node_distance(i, nid);
		}
	}
}

int find_closest_node(int nid, int *cnid)
{
	int i;
	int closest_nid = -1;
	int closest_nid_no_cpu = -1;
	int min_distance = INT_MAX;
	int min_distance_no_cpu = INT_MAX;

	if (nid < 0 || nid >= ARRAY_SIZE(acpi_mem_cached) ||
	    !acpi_mem_cached[nid].online) {
		return -EINVAL;
	}

	for (i = 0; i < SMAP_MAX_NUMNODES; i++) {
		if (i == nid || acpi_mem_cached[nid].distance[i] == 0) {
			continue;
		}
		if (acpi_mem_cached[nid].distance[i] < min_distance) {
			min_distance = acpi_mem_cached[nid].distance[i];
			closest_nid = i;
		}
		if (!node_state(i, N_CPU) &&
		    acpi_mem_cached[nid].distance[i] < min_distance_no_cpu) {
			min_distance_no_cpu = acpi_mem_cached[nid].distance[i];
			closest_nid_no_cpu = i;
		}
	}
	if (closest_nid == -1)
		return -ENODATA;
	*cnid = (closest_nid_no_cpu != -1) ? closest_nid_no_cpu : closest_nid;
	return 0;
}

void print_acpi_mem(void)
{
	struct acpi_mem_segment *mem;
	list_for_each_entry(mem, &acpi_mem.mem, segment) {
		pr_debug("[%d] %#llx-%#llx\n", mem->node, mem->start, mem->end);
	}
}

int init_acpi_mem(void)
{
	int count;
	int last_node = -1;
	acpi_status status;
	struct acpi_mem_segment *mem;
	struct acpi_table_header *table_header = NULL;
	unsigned long table_size = sizeof(struct acpi_table_srat);
	struct acpi_subtable_proc proc = {
		.id = ACPI_SRAT_TYPE_MEMORY_AFFINITY,
		.handler = acpi_parse_memory_affinity,
	};

	if (acpi_disabled) {
		pr_warn("acpi disabled\n");
		return -ENODEV;
	}

	status = acpi_get_table(ACPI_SIG_SRAT, 0, &table_header);
	if (ACPI_FAILURE(status) || !table_header) {
		pr_warn("%4.4s not present\n", ACPI_SIG_SRAT);
		return -ENODEV;
	}

	count = acpi_parse_entries_array(ACPI_SIG_SRAT, table_size,
					 table_header, &proc, 1, 0);
	if (count < 0) {
		pr_err("acpi parse entries array failed, ret is %d\n", count);
		acpi_put_table(table_header);
		return -EINVAL;
	}

	acpi_put_table(table_header);

	list_for_each_entry(mem, &acpi_mem.mem, segment) {
		if (mem->node >= ARRAY_SIZE(acpi_mem_cached))
			return -ERANGE;
		/*
		 * if memory belongs to same node was split into multiple
		 * segment, clear the start and end
		 */
		if (mem->node == last_node) {
			acpi_mem_cached[mem->node].start = 0;
			acpi_mem_cached[mem->node].end = 0;
		} else {
			acpi_mem_cached[mem->node].online = true;
			acpi_mem_cached[mem->node].start = mem->start;
			acpi_mem_cached[mem->node].end = mem->end;
			last_node = mem->node;
		}
		pr_info("node %d pxm %d [mem %#llx-%#llx]\n", mem->node,
			mem->pxm, mem->start, mem->end);
		if (mem->node >= nr_local_numa) {
			nr_local_numa = mem->node + 1;
			pr_info("nr_local_numa is %u\n", nr_local_numa);
		}
	}
	calc_node_distance();

	return 0;
}

void reset_acpi_mem(void)
{
	struct acpi_mem_segment *mem, *tmp;
	list_for_each_entry_safe(mem, tmp, &acpi_mem.mem, segment) {
		list_del(&mem->segment);
		kfree(mem);
	}
	acpi_mem.len = 0;
}

int get_node_actc_len(int len, u64 *node_actc_len)
{
	int i;
	u64 addr_range;
	u64 page_cnt;
	struct acpi_mem_segment *mem;

	if (len == 0) {
		return -EINVAL;
	}

	for (i = 0; i < len; i++)
		node_actc_len[i] = 0;

	list_for_each_entry(mem, &acpi_mem.mem, segment) {
		if (unlikely(mem->node >= len))
			continue;
		addr_range = mem->end - mem->start + 1;
		if (is_smap_pg_huge())
			page_cnt = calc_2m_count(addr_range);
		else
			page_cnt = calc_4k_count(addr_range);
		node_actc_len[mem->node] += page_cnt;
	}

	return 0;
}

int calc_paddr_acidx(u64 paddr, int *nid, u64 *index)
{
	struct acpi_mem_segment *mem;
	bool found = false;
	u64 offset = 0;
	u64 acidx;
	int last_nid = -1;
	int shift = is_smap_pg_huge() ? TWO_MEGA_SHIFT : PAGE_SHIFT;

	list_for_each_entry(mem, &acpi_mem.mem, segment) {
		if (last_nid != mem->node) {
			offset = 0;
			last_nid = mem->node;
		}
		if (paddr > mem->end) {
			offset += mem->end - mem->start + 1;
			continue;
		} else if (unlikely(paddr < mem->start)) {
			break;
		}
		offset += paddr - mem->start;
		acidx = offset >> shift;
		found = true;
		break;
	}
	if (!found)
		return -ERANGE;
	*nid = mem->node;
	*index = acidx;
	return 0;
}
