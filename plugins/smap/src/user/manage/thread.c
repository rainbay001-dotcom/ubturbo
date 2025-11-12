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
#define _GNU_SOURCE
#include <unistd.h>

#include "smap_user_log.h"
#include "manage.h"
#include "thread.h"

#define CHECK_THREAD_INTERVAL 2000

static void *ThreadMain(void *args)
{
    ThreadCtx *ctx = args;

    SMAP_LOGGER_INFO("Thread %lu created.", gettid());
    while (!EnvAtomicRead(&ctx->stop)) {
        EnvMsleep(ctx->period);
        ctx->workFunc(ctx);
    }
    return NULL;
}

int InitThread(struct ProcessManager *manager, uint32_t period, WorkFunc workFunc)
{
    ThreadCtx *ctx = malloc(sizeof(ThreadCtx));
    if (!ctx) {
        SMAP_LOGGER_ERROR("Alloc mem for thread failed.");
        return -ENOMEM;
    }
    ctx->period = period;
    EnvAtomicSet(&ctx->stop, 0);
    ctx->processManager = manager;
    ctx->workFunc = workFunc;
    manager->threadCtx[manager->nrThread] = ctx;
    pthread_create(&ctx->thread, NULL, ThreadMain, ctx);
    manager->nrThread++;
    return 0;
}

int DestroyAllThread(struct ProcessManager *manager)
{
    uint16_t nrThread = manager->nrThread;
    for (int i = 0; i < nrThread; i++) {
        ThreadCtx *ctx = manager->threadCtx[i];
        if (ctx) {
            EnvAtomicSet(&ctx->stop, 1);
            pthread_join(ctx->thread, NULL);
            free(ctx);
            manager->threadCtx[i] = NULL;
            manager->nrThread--;
            SMAP_LOGGER_INFO("Thread%d destroyed.", i);
        }
    }
    return 0;
}
