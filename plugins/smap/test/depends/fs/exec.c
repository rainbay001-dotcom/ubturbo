// SPDX-License-Identifier: GPL-2.0-only
#include <linux/types.h>
#include <linux/sched.h>


char *__get_task_comm(char *buf, size_t buf_size, struct task_struct *tsk)
{
	return buf;
}
