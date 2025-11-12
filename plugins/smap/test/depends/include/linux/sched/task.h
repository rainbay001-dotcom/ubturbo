/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_TASK_H
#define _LINUX_SCHED_TASK_H

#include <linux/sched.h>

extern struct task_struct init_task;

#define task_lock(x)
#define task_unlock(x)

#ifdef __cplusplus
extern "C" {
#endif

extern void put_task_struct(struct task_struct *t);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_SCHED_TASK_H */
