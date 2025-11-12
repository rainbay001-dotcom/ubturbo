/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

#include <linux/list.h>
#include <linux/spinlock_types.h>

#define wake_up_interruptible(x) (x)
#define init_waitqueue_head(x) (x)

#define wait_event_interruptible_timeout(wq_head, condition, timeout) (0)

struct wait_queue_head {
	spinlock_t lock;
	struct list_head head;
};

typedef struct wait_queue_head wait_queue_head_t;

#endif /* _LINUX_WAIT_H */
