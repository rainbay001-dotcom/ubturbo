/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_NUMA_H
#define _LINUX_NUMA_H
#include <linux/pfn.h>
#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int __node_distance(int from, int to);
#define node_distance(a, b) __node_distance(a, b)

#define MAX_NUMNODES    (1 << NODES_SHIFT)

#define	NUMA_NO_NODE	(-1)

#ifdef CONFIG_NUMA_KEEP_MEMINFO
#define __initdata_or_meminfo
#else
#define __initdata_or_meminfo __initdata
#endif

#ifdef CONFIG_NODES_SHIFT
#define NODES_SHIFT     CONFIG_NODES_SHIFT
#else
#define NODES_SHIFT     0
#endif

#ifdef CONFIG_NUMA
#include <linux/printk.h>
#include <asm/sparsemem.h>

int numa_map_to_online_node(int node);

#ifndef phys_to_target_node
static inline int phys_to_target_node(u64 start_addr)
{
	pr_info_once("Unknown target node for memory at 0x%llx, assuming node 0\n", start_addr);
	return 0;
}
#endif
#ifndef memory_add_physaddr_to_nid
static inline int memory_add_physaddr_to_nid(u64 start_addr)
{
	pr_info_once("Unknown online node for memory at 0x%llx, assuming node 0\n", start_addr);
	return 0;
}
#endif
#else
static inline int phys_to_target_node(u64 start_addr)
{
	return 0;
}
static inline int memory_add_physaddr_to_nid(u64 start_addr)
{
	return 0;
}
static inline int numa_map_to_online_node(int node)
{
	return NUMA_NO_NODE;
}
#endif

#ifdef CONFIG_HAVE_ARCH_NODE_DEV_GROUP
extern const struct attribute_group arch_node_dev_group;
#endif

#ifdef __cplusplus
}
#endif

#endif
