/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_NODEMASK_H
#define __LINUX_NODEMASK_H

#include <linux/numa.h>
#include <linux/bitops.h>
#include <linux/minmax.h>

typedef struct { unsigned long bits[BITS_TO_LONGS(MAX_NUMNODES)]; } nodemask_t;

#ifdef __cplusplus
extern "C" {
#endif


enum node_states {
	N_POSSIBLE,
	N_ONLINE,
	N_NORMAL_MEMORY,
#ifdef CONFIG_HIGHMEM
	N_HIGH_MEMORY,
#else
	N_HIGH_MEMORY = N_NORMAL_MEMORY,
#endif
	N_MEMORY,
	N_CPU,
	N_GENERIC_INITIATOR,
	NR_NODE_STATES
};

extern nodemask_t node_states[NR_NODE_STATES];
extern int node_state(int node, enum node_states state);

#define node_isset(node, nodemask) test_bit((node), (nodemask).bits)

#define next_node(node, source) __next_node((node), &(source))
#define first_node(source) __first_node(&(source))
static inline unsigned int __first_node(const nodemask_t *source_p)
{
	return min_t(unsigned int, find_first_bit(source_p->bits, MAX_NUMNODES), MAX_NUMNODES);
}

static inline unsigned int __next_node(int node, const nodemask_t *source_p)
{
	return min_t(unsigned int, find_next_bit(source_p->bits, MAX_NUMNODES, node+1), MAX_NUMNODES);
}

#define nodes_clear(dst) __nodes_clear(&(dst), MAX_NUMNODES)
static inline void __nodes_clear(nodemask_t *dstp, unsigned int nbits)
{
}

#define node_set(n, dst) __node_set((n), &(dst))
static __always_inline void __node_set(int n, volatile nodemask_t *dstp)
{
}

#define node_possible(n)	node_state((n), N_POSSIBLE)

#if MAX_NUMNODES > 1
#define for_each_node_mask(n, m)				    \
	for ((n) = first_node(m); (n >= 0) && (n) < MAX_NUMNODES; (n) = next_node((n), (m)))
#else
#define for_each_node_mask(n, m)                                  \
	for ((n) = 0; (n) < 1 && !nodes_empty(m); (n)++)
#endif

#define for_each_node_state(__node, __state) \
	for_each_node_mask((__node), node_states[__state])

#ifdef __cplusplus
}
#endif


#endif /* __LINUX_NODEMASK_H */
