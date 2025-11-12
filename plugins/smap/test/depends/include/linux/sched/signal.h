/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_SIGNAL_H
#define _LINUX_SCHED_SIGNAL_H

#include <linux/rculist.h>
#include <linux/rwsem.h>
#include <linux/sched/task.h>
#include <linux/sched.h>

#define next_task(p) \
	list_entry_rcu((p)->tasks.next, struct task_struct, tasks)

#define for_each_process(p) \
	for ((p) = &init_task ; ((p) = next_task(p)) != &init_task ;)

struct signal_struct {
	struct rw_semaphore exec_update_lock;
	struct mm_struct *oom_mm;	/* recorded mm when the thread group got
					 * killed by the oom killer */
};

#endif /* _LINUX_SCHED_SIGNAL_H */
