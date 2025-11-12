/* SPDX-License-Identifier: GPL-2.0 */
/*
 * workqueue.h --- work queue handling for Linux.
 */

#ifndef _LINUX_WORKQUEUE_H
#define _LINUX_WORKQUEUE_H

#include <linux/atomic/atomic-long.h>
#include <linux/timer.h>
#include <linux/completion.h>

enum {
    __WQ_ORDERED_EXPLICIT = 1 << 19,
    __WQ_LEGACY = 1 << 18,
    __WQ_ORDERED = 1 << 17,
	__WQ_DRAINING = 1 << 16,

    WQ_POWER_EFFICIENT	= 1 << 7,
    WQ_SYSFS = 1 << 6,
    WQ_CPU_INTENSIVE = 1 << 5,
    WQ_HIGHPRI = 1 << 4,
    WQ_MEM_RECLAIM = 1 << 3,
    WQ_FREEZABLE = 1 << 2,
	WQ_UNBOUND = 1 << 1,

    WQ_MAX_UNBOUND_PER_CPU = 4,
	WQ_MAX_ACTIVE = 512,
	WQ_DFL_ACTIVE = WQ_MAX_ACTIVE / 2,
};

#define INIT_WORK(_work, _func)
#define INIT_DELAYED_WORK(_work, _func)

#define create_workqueue(name) alloc_workqueue("%s", __WQ_LEGACY | WQ_MEM_RECLAIM, 1, (name))

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	atomic_long_t data; // atomic_long_t -> atomic64_t
	struct list_head entry;
	work_func_t func;
#ifdef CONFIG_LOCKDEP
	struct lockdep_map lockdep_map;
#endif
};

static inline bool cancel_work_sync(struct work_struct *work)
{
    return true;
}

static inline bool schedule_work(struct work_struct *work)
{
    return true;
}

struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags, int max_active, ...);

struct timer_list;
struct delayed_work {
	struct work_struct work;    // main work
	struct timer_list timer;
	struct workqueue_struct *wq;
	int cpu;
};

static inline struct delayed_work *to_delayed_work(struct work_struct *work)
{
    return container_of(work, struct delayed_work, work);
}

#define flush_workqueue(wq) \
    do {                    \
    } while (0)

#endif