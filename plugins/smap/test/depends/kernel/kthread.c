/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/kthread.h>

struct task_struct *kthread_create_on_node(int (*threadfn)(void *data),
					   void *data, int node,
					   const char namefmt[],
					   ...)
{
	return NULL;
}

bool kthread_should_stop(void)
{
	return true;
}

int kthread_stop(struct task_struct *k)
{
	return 0;
}