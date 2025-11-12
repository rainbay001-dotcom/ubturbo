/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 dump info module
 */

#ifndef _SRC_DUMP_INFO_H
#define _SRC_DUMP_INFO_H

#include "common.h"
#include "smap_msg.h"

#define MAX_LOG_SIZE (1UL << 30)
#define BUF_MAX_SIZE (1UL << 15)
#define FILENAME_LEN 100
#define TIME_STR_LEN 30
#define TZ_OFFSET (8 * 60 * 60)
#define EPOCH (1900L)

extern size_t nr_abnormal[NR_ABNORMAL];

int check_and_create_dir(char *dir);
int check_filesize(char *filepath, loff_t max_sz, int *exceed);
int rename_file(char *old_name, char *new_name);
int backup_file_if_needed(char *dir, char *name, loff_t max_sz);
void filter_4k_migrate_info(void);

#endif /* _SRC_DUMP_INFO_H */
