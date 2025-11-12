/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LINUX_SPINLOCK_TYPES_H
#define __LINUX_SPINLOCK_TYPES_H


#include <asm/spinlock_types.h>
#include <linux/lockdep_types.h>

typedef struct raw_spinlock {
    // The underlying architecture-specific spinlock
	arch_spinlock_t raw_lock;
} raw_spinlock_t;

#define ___SPIN_LOCK_INITIALIZER(lockname)	\
	{					\
	.raw_lock = __ARCH_SPIN_LOCK_UNLOCKED }

typedef struct spinlock {
    // Union to hold the raw spinlock
	union {
		struct raw_spinlock rlock;
	};
} spinlock_t;

#define __SPIN_LOCK_INITIALIZER(lockname) \
	{ { .rlock = ___SPIN_LOCK_INITIALIZER(lockname) } }

#define __SPIN_LOCK_UNLOCKED(lockname) \
	(spinlock_t) __SPIN_LOCK_INITIALIZER(lockname)

#define DEFINE_SPINLOCK(x)	spinlock_t x = __SPIN_LOCK_UNLOCKED(x)

#endif /* __LINUX_SPINLOCK_TYPES_H */
