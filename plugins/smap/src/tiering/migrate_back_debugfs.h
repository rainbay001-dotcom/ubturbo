/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap migrate back debugfs module
 */

#ifndef _SRC_TIERING_MIGRATE_BACK_DEBUGFS_H
#define _SRC_TIERING_MIGRATE_BACK_DEBUGFS_H

#include <linux/module.h>
#include <linux/fs.h>

#include "migrate_task.h"
#include "smap_msg.h"

extern struct dentry *dbg_root;

void create_migrate_back_debugfs(struct migrate_back_task *task);
void remove_migrate_back_debugfs(struct migrate_back_task *task);
ssize_t smap_migrate_back_debugfs_read(struct file *filp, char __user *user_buf,
				       size_t count, loff_t *ppos);

#endif /* _SRC_TIERING_MIGRATE_BACK_DEBUGFS_H */
