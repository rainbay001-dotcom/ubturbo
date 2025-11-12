// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap migrate back debugfs module
 */

#define DEBUGFS_RO_MODE 0444
#define BUF_MAX_SIZE 256
#define NAME_MAX_LEN 50
#define PREFIX "mb"

#include <linux/debugfs.h>

#include "migrate_task.h"
#include "migrate_back_debugfs.h"

ssize_t smap_migrate_back_debugfs_read(struct file *filp, char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	char buf[BUF_MAX_SIZE];
	ssize_t ret;
	struct migrate_back_task *task = filp->private_data;

	ret = scnprintf(buf, BUF_MAX_SIZE, "%d\n%u\n", task->status,
			task->progress);
	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

int smap_migrate_back_debugfs_open(struct inode *inode, struct file *filp)
{
	if (inode->i_private) {
		filp->private_data = inode->i_private;
	}
	return 0;
}

struct file_operations smap_migrate_back_debugfs_ops = {
	.owner = THIS_MODULE,
	.open = smap_migrate_back_debugfs_open,
	.read = smap_migrate_back_debugfs_read,
	.llseek = default_llseek,
};

void create_migrate_back_debugfs(struct migrate_back_task *task)
{
	char file_name[NAME_MAX_LEN];
	struct dentry *dentry;

	int rc = scnprintf(file_name, NAME_MAX_LEN, "%s_%llu", PREFIX,
			   task->task_id);
	if (!rc) {
		pr_err("create debugfs write file name: %s_%llu failed\n",
		       PREFIX, task->task_id);
		return;
	}
	dentry = debugfs_lookup(file_name, dbg_root);
	if (dentry) {
		debugfs_remove(dentry);
	}
	debugfs_create_file(file_name, DEBUGFS_RO_MODE, dbg_root, task,
			    &smap_migrate_back_debugfs_ops);
}

void remove_migrate_back_debugfs(struct migrate_back_task *task)
{
	char file_name[NAME_MAX_LEN];
	struct dentry *dentry;

	int rc = scnprintf(file_name, NAME_MAX_LEN, "%s_%llu", PREFIX,
			   task->task_id);
	if (!rc) {
		pr_err("remove debugfs write file name: %s_%llu failed\n",
		       PREFIX, task->task_id);
		return;
	}
	dentry = debugfs_lookup(file_name, dbg_root);
	if (dentry) {
		debugfs_remove(dentry);
	}
}
