/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_CONTAINER_OF_H
#define _LINUX_CONTAINER_OF_H

#include <linux/err.h>

#define typeof_member(type, member)	typeof(((type*)0)->member)

#define container_of(p, type, member) ({				\
	void *__mptr = (void *)(p);					\
	((type *)(__mptr - offsetof(type, member))); })

#define container_of_safe(p, type, member) ({				\
	void *__mptr = (void *)(p);					\
	IS_ERR_OR_NULL(__mptr) ? ERR_CAST(__mptr) :			\
		((type *)(__mptr - offsetof(type, member))); })

#endif
