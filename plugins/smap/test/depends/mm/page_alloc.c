/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/nodemask.h>

// Node mask for various node states
nodemask_t node_states[NR_NODE_STATES] = {
    // Nodes that are currently online
    [N_ONLINE] = { { [0] = 1UL } },

#ifndef CONFIG_NUMA
    // Nodes that contain normal memory (when NUMA is not configured)
    [N_NORMAL_MEMORY] = { { [0] = 1UL } },

    // Nodes that contain high memory (when high memory is configured and NUMA is not)
    #ifdef CONFIG_HIGHMEM
    [N_HIGH_MEMORY] = { { [0] = 1UL } },
    #endif

    // Nodes that contain any type of memory (when NUMA is not configured)
    [N_MEMORY] = { { [0] = 1UL } },

    // Nodes that contain CPUs (when NUMA is not configured)
    [N_CPU] = { { [0] = 1UL } },
#endif	/* NUMA */
};
struct page *pfn_to_page(unsigned long pfn)
{
    return NULL;
}

struct page *pfn_to_online_page(unsigned long pfn)
{
    return NULL;
}

bool pfn_valid(unsigned long pfn)
{
	return true;
}

int page_to_nid(const struct page *page)
{
	return 0;
}