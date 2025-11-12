/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COMPLETION_H
#define __LINUX_COMPLETION_H

#include <linux/swait.h>

struct completion {
	unsigned int done;
	struct swait_queue_head wait;
};

#define init_completion(x) __init_completion(x)

static inline void __init_completion(struct completion *x)
{
	x->done = 0;
}

static inline void reinit_completion(struct completion *x)
{
	x->done = 0;
}

void complete(struct completion *);
void wait_for_completion(struct completion *x);
int wait_for_completion_killable(struct completion *x);

#endif
