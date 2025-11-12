/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SWAIT_H
#define _LINUX_SWAIT_H

#include <linux/list.h>
#include <linux/spinlock.h>

struct swait_queue_head {
	raw_spinlock_t		lock;
	struct list_head	task_list;
};

#endif /* _LINUX_SWAIT_H */
