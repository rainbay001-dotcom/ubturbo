// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Debugfs
 */
#include "smap_debugfs.h"

#define DEBUGFS_MIGRATE_ROOT_PATH "smap"

struct dentry *dbg_root, *dbg_tiering, *dbg_vm, *dbg_pid;

int smap_debugfs_migrate_init(void)
{
	int ret = 0;

	dbg_root = debugfs_create_dir(DEBUGFS_MIGRATE_ROOT_PATH, NULL);
	if (IS_ERR(dbg_root)) {
		return PTR_ERR(dbg_root);
	}

	return ret;
}

void smap_debugfs_mod_exit(void)
{
	debugfs_remove_recursive(dbg_root);
}
