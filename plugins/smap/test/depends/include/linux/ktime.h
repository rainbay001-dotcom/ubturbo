/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_KTIME_DEPENDS_H
#define _LINUX_KTIME_DEPENDS_H

#define KTIME_MAX			((s64)~((u64)1 << 63))
#define NSEC_PER_USEC	1000L
#define NSEC_PER_MSEC	1000000L

typedef s64	ktime_t;

static inline ktime_t ktime_get(void)
{
	return 0;
}

static inline s64 ktime_to_us(const ktime_t kt)
{
	return kt / NSEC_PER_USEC;
}

static inline s64 ktime_to_ms(const ktime_t kt)
{
	return kt / NSEC_PER_MSEC;
}

#endif
