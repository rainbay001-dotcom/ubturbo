/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/mmap_lock.h>

int mmap_read_lock_killable(struct mm_struct *mm)
{
	return 0;
}

void mmap_assert_locked(struct mm_struct *mm)
{}

void mmap_read_unlock(struct mm_struct *mm)
{}

void mmap_read_lock(struct mm_struct *mm)
{}