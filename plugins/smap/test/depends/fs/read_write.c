/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/fs.h>

ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)
{
	return 0;
}

ssize_t kernel_write(struct file *file, void *buf, size_t count, loff_t *pos)
{
	return 0;
}

loff_t default_llseek(struct file *file, loff_t offset, int whence)
{
	return 0;
}

int iterate_dir(struct file *, struct dir_context *)
{
	return 0;
}
