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

#ifndef __SMAP_ENV_H__
#define __SMAP_ENV_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>

#ifdef CONFIG_X86
#define VM_NAME_STR "qemu-system-x86"
#else
#define VM_NAME_STR "qemu-system-aar"
#define VM_KVM_NAME_STR "qemu-kvm"
#endif

#define PID_NAME_LEN (15)
#define PID_KVM_NAME_LEN (8)
#define CMDLINE_LEN 4096
#define PATH_MAX 4096
#define HUNDRED 100
#define LITTLE_NUM (1.0e-10)
#define USEC_PER_MSEC 1000

/* Miscellaneous defines */
#define KIB (1ULL << 10)
#define MIB (1ULL << 20)
#define GIB (1ULL << 30)
#define TIB (1ULL << 40)

#define PAGETYPE_NORMAL 0
#define PAGETYPE_HUGE 1

#define PAGESIZE_4K (4 * KIB)
#define PAGESIZE_64K (64 * KIB)
#define PAGESIZE_2M (2 * MIB)

#define MAX_NR_MIGOUT 40

#define CAT_SCRIPT_CAT_PATH "sudo /usr/local/bin/cat.sh"
#define CAT_SCRIPT_TAIL "2>&1"

#ifdef __cplusplus
typedef std::atomic<int> atomic_int;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MIG_RATIO_MODE = 0, // 按照比例迁移
    MIG_MEMSIZE_MODE, // 按照内存大小迁移
} MigrateMode;

/* MUTEX */
typedef struct {
    pthread_mutex_t lock;
} EnvMutex;

static inline int EnvMutexDestroy(EnvMutex *mutex)
{
    if (pthread_mutex_destroy(&mutex->lock)) {
        return 1;
    }

    return 0;
}

static inline int EnvMutexInit(EnvMutex *mutex)
{
    if (pthread_mutex_init(&mutex->lock, NULL)) {
        return 1;
    }

    return 0;
}

static inline void EnvMutexLock(EnvMutex *mutex)
{
    pthread_mutex_lock(&mutex->lock);
}

static inline void EnvMutexUnlock(EnvMutex *mutex)
{
    pthread_mutex_unlock(&mutex->lock);
}

typedef struct {
    atomic_int counter;
} EnvAtomic;

static inline int EnvAtomicRead(const EnvAtomic *a)
{
    return atomic_load(&a->counter);
}

static inline void EnvAtomicSet(EnvAtomic *a, int i)
{
    atomic_store(&a->counter, i);
}

static inline int EnvAtomicCmpAndSwap(int oldValue, int newValue, EnvAtomic *a)
{
#ifdef __cplusplus
    if (a->counter.compare_exchange_strong(oldValue, newValue)) {
        return oldValue;
    }
    return newValue;
#else
    return __sync_val_compare_and_swap(&a->counter, oldValue, newValue); // C 中使用内置函数
#endif
}
/* TIME */
static inline void EnvMsleep(uint64_t n)
{
    usleep(n * USEC_PER_MSEC);
}

#ifdef __cplusplus
}
#endif
#endif /* __SMAP_ENV_H__ */
