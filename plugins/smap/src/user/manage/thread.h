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
#ifndef __THREAD_H__
#define __THREAD_H__

#include "smap_env.h"

struct ThreadContext {
    EnvAtomic stop;
    struct ProcessManager *processManager;
    uint32_t period;
    pthread_t thread;
    int (*workFunc)(struct ThreadContext *priv);
};

typedef struct ThreadContext ThreadCtx;

typedef int (*WorkFunc)(ThreadCtx *priv);

int InitThread(struct ProcessManager *manager, uint32_t period, WorkFunc workFunc);

int DestroyAllThread(struct ProcessManager *manager);

#endif /* __THREAD_H__ */