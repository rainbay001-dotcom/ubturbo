/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/mm_types.h>


bool mmget_not_zero(struct mm_struct *mm)
{
	return true;
}
