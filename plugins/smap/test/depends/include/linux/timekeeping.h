/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _LINUX_KTIME_KEEPING_H
#define _LINUX_KTIME_KEEPING_H

#include <linux/types.h>
#include <linux/ktime.h>

typedef s64	ktime_t;

#define ktime_sub(lhs, rhs)	((lhs) - (rhs))

#define ktime_add(lhs, rhs)	((lhs) + (rhs))

static inline s64 ktime_get_real(void)
{
    return 0;
}

static inline ktime_t ktime_get_boottime(void)
{
    return 0;
}

#endif //_LINUX_KTIME_KEEPING_H
