// SPDX-License-Identifier: GPL-2.0-only
/*
 *  kernel/sched/core.c
 *
 *  Core kernel scheduler code and related syscalls
 *
 *  Copyright (C) 1991-2002  Linus Torvalds
 */
#include <linux/sched.h>

void schedule(void)
{
}

int wake_up_process(struct task_struct *p)
{
	return 0;
}