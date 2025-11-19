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

#ifndef __SEPARATE_STRATEGY_H__
#define __SEPARATE_STRATEGY_H__

#include "manage/manage.h"

#define PERCENTAGE_BASE_INT 100
#define PERCENTAGE_BASE_DOUBLE 100.0

#define STRATEGY_MIN_HOT_COLD_THRED_VALUE 2
#define STRATEGY_FREQ_THRED_DIVISOR 5
#define STRATEGY_ACTC_MAX_FREQ 256

typedef enum { SELECT_TOP_K, SELECT_BOTTOM_K } SelectionMode;

typedef enum {
    SEPARATE_NONE,
    SEPARATE_NORMAL,
    SEPARATE_CPI,
    SEPARATE_OPTIMIZE,
    SEPARATE_HDT,
} SeparateStrategyType;

typedef struct {
    uint64_t nrMig;
    MigrateDirection dir;
} RemoteMigInfo;

int SeparateStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES]);
int SeparateStrategy4K(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES]);
int SeparateStrategyMultiNumaVm(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES]);
#endif /* __SEPARATE_STRATEGY_H__ */
