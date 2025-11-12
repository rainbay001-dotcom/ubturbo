/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __ASM_DEPENDS_GENERIC_QSPINLOCK_TYPES_H
#define __ASM_DEPENDS_GENERIC_QSPINLOCK_TYPES_H

#include <linux/types.h>

typedef struct qspinlock {
	union {
		struct {
			u8	pending;
			u8	locked;
			u8	reserved[2];
		};
		atomic_t val;
		struct {
			u16	tail;
            u16	locked_pending;
		};
	};
} arch_spinlock_t;

#define	__ARCH_SPIN_LOCK_UNLOCKED	{ { .val = ATOMIC_INIT(0) } }

#endif
