/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_CGROUP_DEFS_H
#define _LINUX_CGROUP_DEFS_H

#include <linux/workqueue.h>

struct cgroup_subsys;

#define SUBSYS(_val) _val ## _cgrp_id,
enum cgroup_subsys_id {
#include <linux/cgroup_subsys.h>
	CGROUP_SUBSYS_COUNT = 0,
};
#undef SUBSYS

struct cgroup_file {
        struct kernfs_node *kn;
        unsigned long notified_at;
        struct timer_list notify_timer;
};

struct cgroup_subsys_state {
	struct cgroup_subsys_state *parent;
};

struct cgroup_subsys {
};

#endif
