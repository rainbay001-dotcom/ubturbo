/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/pid.h>

struct task_struct *get_pid_task(struct pid *pid, enum pid_type type)
{
	return NULL;
}


void put_task_struct(struct task_struct *t)
{
}

struct pid *find_vpid(int nr)
{
	return NULL;
}

struct pid *find_get_pid(pid_t nr)
{
	return NULL;
}

void put_pid(struct pid *pid)
{
}

struct task_struct *find_get_task_by_vpid(pid_t nr)
{
    return NULL;
}
