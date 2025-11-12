/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_MMAP_LOCK_H
#define _LINUX_MMAP_LOCK_H

#include <linux/mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

int mmap_read_lock_killable(struct mm_struct *mm);

void mmap_assert_locked(struct mm_struct *mm);

void mmap_read_unlock(struct mm_struct *mm);

void mmap_read_lock(struct mm_struct *mm);

static inline void mmap_write_lock(struct mm_struct *mm)
{
}

static inline void mmap_write_unlock(struct mm_struct *mm)
{
}

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_MMAP_LOCK_H */
