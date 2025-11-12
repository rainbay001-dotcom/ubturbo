/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __MIGRATION_H__
#define __MIGRATION_H__

#include "smap_env.h"
#include "manage/manage.h"
#include "manage/thread.h"

#define DEFAULT_FROM_NODE (-1)
#define DEFAULT_TO_NODE (-1)

#define SIG_THREAD_MIG_OUT 1
#define LESS_THREAD_MIG_OUT 4
#define MORE_THREAD_MIG_OUT 8
#define LESS_MIG_OUT_2M_PAGE_THRE 40
#define MORE_MIG_OUT_2M_PAGE_THRE 400
#define MAX_PER_PID_MIG_LIST_COUNT 8
#define REMOTE_MIG_FAIL 92

int AddMigList(struct MigrateMsg *mMsg, struct MigList *mList);

void UpdateMigResult(struct MigrateMsg *mMsg, struct ProcessManager *manager);

int DoMigration(struct MigrateMsg *mMsg, struct ProcessManager *manager);

int ScanMigrateWork(ThreadCtx *ctx);

int MigrateRemoteNuma(struct ProcessManager *manager, struct MigrateNumaIoctlMsg *msg);
#endif /* __MIGRATION_H__ */