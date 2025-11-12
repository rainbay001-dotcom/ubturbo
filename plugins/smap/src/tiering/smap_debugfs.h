/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Debugfs接口文件
 */

#ifndef SMAP_DEBUGFS_H
#define SMAP_DEBUGFS_H

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/fs.h>

int smap_debugfs_migrate_init(void);

void smap_debugfs_mod_exit(void);

#endif