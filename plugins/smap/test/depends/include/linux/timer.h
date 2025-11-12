/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/list.h>
#include <linux/stringify.h>

struct timer_list {
    // Expiration time of the timer, indicating when the timer will trigger
    unsigned long		expires;
	struct hlist_node	entry;
	void			(*function)(struct timer_list *);
    // Flags to control the behavior of the timer
	u32			flags;

#ifdef CONFIG_LOCKDEP
    // Structure used for lock dependency checking in the kernel
	struct lockdep_map	lockdep_map;
#endif
};

#endif