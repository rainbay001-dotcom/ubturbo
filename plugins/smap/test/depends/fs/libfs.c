/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/compiler_types.h>

ssize_t simple_read_from_buffer(void __user *to, size_t count, loff_t *ppos,
				const void *from, size_t available)
{
	return 0;
}

ssize_t simple_write_to_buffer(void *to, size_t available, loff_t *ppos,
		const void __user *from, size_t count)
{
}
