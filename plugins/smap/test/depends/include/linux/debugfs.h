// SPDX-License-Identifier: GPL-2.0

#ifndef _DEBUGFS_DEPENDS_H_
#define _DEBUGFS_DEPENDS_H_

#include <linux/seq_file.h>
#include <linux/fs.h>
#include <asm-generic/int-ll64.h>

#ifdef __cplusplus
extern "C" {
#endif

struct file_operations;
struct device;

struct debugfs_reg32 {
	char *name;
	unsigned long offset;
};

extern struct dentry *arch_debugfs_dir;

struct debugfs_u32_array {
	u32 *array;
	u32 n_elements;
};

struct debugfs_blob_wrapper {
	void *data;
	unsigned long size;
};
struct debugfs_regset32 {
    struct device *dev;
    int nregs;
	const struct debugfs_reg32 *regs;
};

#define DEFINE_DEBUGFS_ATTRIBUTE_XSIGNED(__fops, __get, __set, __fmt, __is_signed)	\
static int __fops ## _open(struct inode *inode, struct file *file_ptr)	\
{									\
	__simple_attr_check_format(__fmt, 0ull);			\
	return simple_attr_open(inode, file_ptr, __get, __set, __fmt);	\
}									\
static const struct file_operations __fops = {				\
    .read	 = debugfs_attr_read,					\
    .release = simple_attr_release,					\
    .open	 = __fops ## _open,					\
	.owner	 = THIS_MODULE,						\
    .llseek  = no_llseek,						\
	.write	 = (__is_signed) ? debugfs_attr_write_signed : debugfs_attr_write,	\
}

#define DEFINE_DEBUGFS_ATTRIBUTE(__fops, __get, __set, __fmt)		\
	DEFINE_DEBUGFS_ATTRIBUTE_XSIGNED(__fops, __get, __set, __fmt, false)

#define DEFINE_DEBUGFS_ATTRIBUTE_SIGNED(__fops, __get, __set, __fmt)	\
	DEFINE_DEBUGFS_ATTRIBUTE_XSIGNED(__fops, __get, __set, __fmt, true)

void debugfs_remove(struct dentry *dentry);
#define debugfs_remove_recursive debugfs_remove

struct dentry *debugfs_lookup(const char *name, struct dentry *parent);

struct dentry *debugfs_create_file(const char *name, umode_t mode, struct dentry *parent, void *data,
                                   const struct file_operations *fops);

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent);

#ifdef __cplusplus
}
#endif
#endif