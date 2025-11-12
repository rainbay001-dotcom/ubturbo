/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/dcache.h>
#include <linux/debugfs.h>

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent)
{
	return NULL;
}

struct dentry *debugfs_create_file(const char *name, umode_t mode,
				   struct dentry *parent, void *data,
				   const struct file_operations *fops)
{
	return NULL;
}

void debugfs_remove(struct dentry *dentry)
{}

struct dentry *debugfs_lookup(const char *name, struct dentry *parent)
{
	return NULL;
}
