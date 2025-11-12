/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/topology.h>

static int numa_distance_cnt;
static u8 *numa_distance;

/*
 * Return NUMA distance @from to @to
 */
int __node_distance(int from, int to)
{
	if (from >= numa_distance_cnt || to >= numa_distance_cnt)
		return from == to ? LOCAL_DISTANCE : REMOTE_DISTANCE;
	return numa_distance[from * numa_distance_cnt + to];
}
EXPORT_SYMBOL(__node_distance);
