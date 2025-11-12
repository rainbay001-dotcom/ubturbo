/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef __UCACHE_H__
#define __UCACHE_H__

#include "ucache_migrate.h"

#ifdef __cplusplus
extern "C" {
#endif
#undef pr_fmt
#define pr_fmt(fmt) "<ucache> " fmt

#define NR_TO_SCAN 10240

#define MAJOR_NUM 100
#define CHRDEV_MAION 0
#define CHRDEV_COUNT 1
#define DEVICE_FILE_NAME "ucache"

struct migrate_info {
	int32_t des_nid;
	int32_t nid;
	pid_t pid;
};

#define UCACHE_QUERY_MIGRATE_SUCCESS \
	_IOWR(MAJOR_NUM, 0, struct migrate_success *)
#define UCACHE_SCAN_MIGRATE_FOLIOS _IOWR(MAJOR_NUM, 1, struct migrate_info *)

#ifdef __cplusplus
}
#endif

#endif
