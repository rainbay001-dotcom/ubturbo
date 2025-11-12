/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_RCULIST_H
#define _LINUX_RCULIST_H

#include <linux/list.h>

#define list_entry_rcu(ptr, type, member) \
	container_of(READ_ONCE(ptr), type, member)

#endif
