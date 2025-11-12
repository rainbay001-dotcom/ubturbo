/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/fs.h>
#include <linux/seq_file.h>

ssize_t seq_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	return 0;
}

loff_t seq_lseek(struct file *file, loff_t offset, int whence)
{
	return 0;
}

void seq_printf(struct seq_file *m, const char *f, ...)
{}

int single_open(struct file *file, int (*show)(struct seq_file *, void *),
		void *data)
{
	return 0;
}

int single_release(struct inode *inode, struct file *file)
{
	return 0;
}
