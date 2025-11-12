/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_REFCOUNT_H
#define _LINUX_REFCOUNT_H

#include <linux/atomic.h>

typedef struct refcount_struct {
    atomic_t refs;
} refcount_t;

#endif
