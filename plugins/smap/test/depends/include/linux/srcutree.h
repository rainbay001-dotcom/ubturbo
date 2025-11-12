/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SRCUTREE_H
#define __SRCUTREE_H

#include <linux/spinlock_types.h>

struct srcu_node {
	spinlock_t lock;
};

struct srcu_struct {
	spinlock_t lock;
	unsigned int srcu_idx;
};

#endif
