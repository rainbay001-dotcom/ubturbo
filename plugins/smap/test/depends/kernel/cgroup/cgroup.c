/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/cgroup-defs.h>

extern struct cgroup_subsys cpuset_cgrp_subsys;

/* generate an array of cgroup subsystem pointers */
#define SUBSYS(_x) [_x ## _cgrp_id] = &_x ## _cgrp_subsys,
struct cgroup_subsys *cgroup_subsys[] = {
#include <linux/cgroup_subsys.h>
};
#undef SUBSYS
