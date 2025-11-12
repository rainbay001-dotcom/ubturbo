/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __LINUX_RCUPDATE_H
#define __LINUX_RCUPDATE_H

static void rcu_read_lock(void)
{}

static void rcu_read_unlock(void)
{}

/* from tools/include/linux/rcu.h */
#define rcu_assign_pointer(p, v)	do { (p) = (v); } while (0)

#endif /* __LINUX_RCUPDATE_H */
