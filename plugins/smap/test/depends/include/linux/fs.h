/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/path.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <stdlib.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <uapi/asm-generic/fcntl.h>

struct file_system_type {
	const char *name;
	int fs_flags;
};

struct super_block {
	struct list_head	s_list;		/* Keep this first */
	dev_t			s_dev;		/* search index; _not_ kdev_t */
	unsigned char		s_blocksize_bits;
	unsigned long		s_blocksize;
	struct file_system_type *s_type;
};

struct inode {
    dev_t			i_rdev;
	void *i_private; /* fs or device private pointer */
	struct super_block *i_sb;
	struct cdev *i_cdev;
};

struct file {
	unsigned int 		f_flags;
	/* needed for tty driver, and maybe others */
	void *private_data;
	struct path		f_path;
	struct inode *f_inode;	/* cached value */
};

struct file_operations {
	long (*unlocked_ioctl) (struct file *f, unsigned int cmd, unsigned long arg);
	int (*release) (struct inode *i, struct file *f);
	int (*open) (struct inode *i, struct file *f);
	ssize_t (*write_iter) (struct kiocb *k, struct iov_iter *iter);
	/* file_operations module owner */
	struct module *owner;
	ssize_t (*read_iter) (struct kiocb *k, struct iov_iter *iter);
	ssize_t (*write) (struct file *f, const char __user *buf, size_t len, loff_t *off);
	ssize_t (*read) (struct file *f, char __user *buf, size_t len, loff_t *off);
	loff_t (*llseek) (struct file *f, loff_t off, int w);
};

struct kiocb {
	struct file *ki_filp;
};

static inline unsigned iminor(const struct inode *inode)
{
    return MINOR(inode->i_rdev);
}

typedef void *fl_owner_t;

#define fput(x)

#ifdef __cplusplus
extern "C" {
#endif
extern int alloc_chrdev_region(dev_t *, unsigned, unsigned, const char *);
extern void unregister_chrdev_region(dev_t, unsigned);
extern ssize_t kernel_read(struct file *file, void *buf, size_t count, loff_t *pos);
extern ssize_t kernel_write(struct file *file, void *buf, size_t count, loff_t *pos);
extern ssize_t simple_read_from_buffer(void __user *to, size_t count,
			loff_t *ppos, const void *from, size_t available);
#ifdef __cplusplus
}
#endif
extern int scnprintf(char *buf, size_t size, const char *fmt, ...);
extern int kstrtouint(const char *s, unsigned int base, unsigned int *res);
extern int kstrtoull(const char *s, unsigned int base, unsigned long long *res);

extern loff_t default_llseek(struct file *file, loff_t offset, int whence);
static inline struct inode *file_inode(const struct file *f)
{
    return 0;
}
static inline loff_t i_size_read(const struct inode *inode)
{
    return 0;
}

struct dir_context;
typedef bool (*filldir_t)(struct dir_context *, const char *, int, loff_t, u64, unsigned);

struct dir_context {
    filldir_t actor;
    loff_t pos;
};
extern int iterate_dir(struct file *, struct dir_context *);

#endif
