/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_CPUSET_H
#define _LINUX_CPUSET_H

#include <linux/cpumask.h>

static inline nodemask_t cpuset_current_mems_allowed_debug(void)
{
    nodemask_t mask;
    node_set(0, mask);
    return mask;
}
#define cpuset_current_mems_allowed cpuset_current_mems_allowed_debug()

static inline void set_mems_allowed(nodemask_t nodemask)
{
}

#endif /* _LINUX_CPUSET_H */
