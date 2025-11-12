/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/sched.h>
#include <linux/mm_types.h>

struct mm_struct *get_task_mm(struct task_struct *task)
{
	return NULL;
}

void mmput(struct mm_struct *mm)
{
}
