/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_SPINLOCK_H
#define __LINUX_SPINLOCK_H

#include <linux/spinlock_types.h>
#include <linux/rwlock.h>

#define spin_lock_irqsave(lock, flags)
#define spin_unlock_irqrestore(lock, flags)

#define spin_lock_init(x)

static __always_inline void spin_lock(spinlock_t *lock)
{
}

static __always_inline void spin_unlock(spinlock_t *lock)
{
}

#endif /* __LINUX_SPINLOCK_H */
