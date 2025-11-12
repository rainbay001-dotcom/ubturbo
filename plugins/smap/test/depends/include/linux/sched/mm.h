/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_MM_H
#define _LINUX_SCHED_MM_H

#include <linux/mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool mmget_not_zero(struct mm_struct *mm);

struct mm_struct *get_task_mm(struct task_struct *task);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_SCHED_MM_H */
