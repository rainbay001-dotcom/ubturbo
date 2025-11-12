/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_CGROUP_H
#define _LINUX_CGROUP_H

#include <linux/nodemask.h>
#include <linux/cgroup-defs.h>

static inline struct cgroup_subsys_state *task_css(struct task_struct *task,
						   int subsys_id)
{
	return NULL;
}

#endif /* _LINUX_CGROUP_H */
