/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The proc filesystem constants/structures
 */
#ifndef _LINUX_PROC_FS_H
#define _LINUX_PROC_FS_H

#include <linux/types.h>

#define proc_create_single(name, mode, parent, show) ({ NULL; })

struct proc_ops {
	unsigned int proc_flags;
	int	(*proc_open)(struct inode *, struct file *);
	ssize_t	(*proc_read)(struct file *, char __user *, size_t, loff_t *);
	/* mandatory unless nonseekable_open() or equivalent is used */
	loff_t	(*proc_lseek)(struct file *, loff_t, int);
};

static inline struct proc_dir_entry *proc_mkdir(const char *name, struct proc_dir_entry *parent)
{
    return (struct proc_dir_entry *)1;
}

static inline struct proc_dir_entry *proc_create(const char *name, umode_t mode, struct proc_dir_entry *parent,
    const struct proc_ops *proc_ops)
{
    return (struct proc_dir_entry *)1;
}

static inline void proc_remove(struct proc_dir_entry *de) {}

struct proc_dir_entry {
    char inline_name[10];
};

#define remove_proc_entry(name, parent) do {} while (0)
#endif /* _LINUX_PROC_FS_H */
