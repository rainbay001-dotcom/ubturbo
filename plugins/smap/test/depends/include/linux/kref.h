/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _KREF_H_
#define _KREF_H_

#include <linux/refcount.h>

struct kref {
    refcount_t refcount;
};

static inline void kref_get(struct kref *kref)
{
}

static inline int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    return 0;
}

static inline void kref_init(struct kref *kref)
{
}

#endif
