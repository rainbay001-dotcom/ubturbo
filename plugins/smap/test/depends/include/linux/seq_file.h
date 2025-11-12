/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_SEQ_FILE_H
#define _LINUX_SEQ_FILE_H

#include <linux/types.h>

struct seq_file {
};

ssize_t seq_read(struct file *, char __user *, size_t, loff_t *);
loff_t seq_lseek(struct file *, loff_t, int);
int single_release(struct inode *, struct file *);

#endif
