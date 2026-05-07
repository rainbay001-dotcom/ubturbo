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
#ifndef __SMAP_INTERFACE_H__
#define __SMAP_INTERFACE_H__

#include <unistd.h>
#include <stdint.h>
#include "manage/smap_config.h"
#include "smap_env.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NR_MIGRATE_ESCAPE 300
#define MAX_NR_MIGBACK 50
#define MAX_NR_MIGNUMA 50
#define MAX_NR_REMOVE MAX_NR_MIGOUT
#define ADAPT_ALLOC_PERIOD 1000
#define SCAN_MIGRATE_PERIOD LIGHT_STABLE_MIGRATE_CYCLE
#define USER_DEFAULT_MIGRATE_OUT_RATIO 25
#define WAIT_TIME (100)
#define MAX_WAIT_TIME (180 * 1000)
#define MIN_WAIT_TIME (10 * 1000)
#define MAX_SCAN_TIME 2000
#define MIN_SCAN_TIME 50
#define DEFAULT_L2_NODE (-1)
/* Sentinel value for ubturbo_smap_migrate_out destNid to enable L1->NVMe swap mode.
 * When passed, SMAP skips L1->L2 migration and directly swaps cold L1 pages to NVMe.
 * Defined in manage/swap_types.h; re-exported here for public API callers. */
#ifndef SMAP_NVME_SWAP_NID
#define SMAP_NVME_SWAP_NID (-2)
#endif
#define KB_PER_2MB 2048
#define MAX_SCAN_DURATION_SEC 300
#define NON_EXIST_PID (-1)

enum {
    SLEEP = 0,
    RUNNING = 1,
};

enum {
    FREE_ADAPT_MEM,
    QUERY_ADAPT_MEM,
};

enum {
    VM_DEVICE_IDX,
    PID_DEVICE_IDX,
    ACCESS_DEVICE_IDX,
    NR_DEVICES,
};

enum {
    DISABLE_NUMA_MIG,
    ENABLE_NUMA_MIG,
};

typedef enum { INPUT_PROCESS = 0, INPUT_VM, INPUT_MAX } InputPidType;

struct MigrateOutPayloadInner {
    int destNid;
    int ratio;
    uint64_t memSize; // 内存迁移大小(KB)
    MigrateMode migrateMode; // 内存迁移模式，按照比例或是大小
};

struct MigrateOutPayload {
    int srcNid; // 是否指定迁出源节点（-1表示不指定）
    pid_t pid;
    int count;
    struct MigrateOutPayloadInner inner[REMOTE_NUMA_NUM];
};

struct MigrateOutMsg {
    int count;
    struct MigrateOutPayload payload[MAX_NR_MIGOUT];
};

struct MigrateBackPayload {
    int srcNid;
    int destNid;
    uint64_t memid;
};

struct MigrateBackMsg {
    unsigned long long taskID;
    int count;
    struct MigrateBackPayload payload[MAX_NR_MIGBACK];
};

struct RemovePayload {
    pid_t pid;
    int count;
    int nid[REMOTE_NUMA_NUM];
};

struct RemoveMsg {
    int count;
    struct RemovePayload payload[MAX_NR_REMOVE];
};

struct EnableNodeMsg {
    int enable;
    int nid;
};

struct SetRemoteNumaInfoMsg {
    int srcNid; // 设置-1时表示每个本地numa都可用此远端numa
    int destNid;
    uint64_t size;
};

struct MigrateEscapePayload {
    pid_t pid;
    int srcNid;
    int destNid;
    int ratio;
    uint64_t memSize;
    MigrateMode migrateMode;
};

struct MigrateEscapeMsg {
    int count;
    struct MigrateEscapePayload payload[MAX_NR_MIGRATE_ESCAPE];
};

enum {
    NORMAL_DATA_SOURCE,
    STATISTIC_DATA_SOURCE,
    MAX_SOURCE,
};

typedef void (*Logfunc)(int level, const char *str, const char *moduleName);

/* *
 * @brief   设置进程迁移到远端NUMA
 *
 * @param msg      [IN] 迁移进程信息，包含迁移进程、远端NUMA和迁移比例
 * @param pidType  [IN] 进程类型，目前支持4KB和2MB进程类型
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_migrate_out(struct MigrateOutMsg *msg, int pidType);

/* *
 * @brief   迁移指定地址段的远端NUMA内存
 *
 * @param msg   [IN] 迁移地址段信息，包含源NUMA、目标NUMA、地址段
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_migrate_back(struct MigrateBackMsg *msg);

/* *
 * @brief   移除进程的冷热页迁移
 *
 * @param msg   [IN] 移除的进程信息，包含进程的PID
 * @param pidType  [IN] 进程类型，目前支持4KB和2MB进程类型
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_remove(struct RemoveMsg *msg, int pidType);

/* *
 * @brief   使能NUMA的冷热页迁移
 *
 * @param msg   [IN] NUMA信息，包含NUMA ID、使能开关
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_node_enable(struct EnableNodeMsg *msg);

/* *
 * @brief   初始化SMAP模块
 *
 * @param pageType [IN] 页面类型，当前只支持4KB和2MB页
 * @param extlog   [IN] 日志打印函数
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_start(uint32_t pageType, Logfunc extlog);

/* *
 * @brief  卸载SMAP模块
 *
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_stop(void);

/* *
 * @brief 紧急迁移本地内存到远端，在发生本地内存无资源场景
 *
 * @param size [IN] 需要紧急迁移的大小
 */
void ubturbo_smap_urgent_migrate_out(uint64_t size);

/* *
 * @brief   设置本地NUMA对远端NUMA迁移内存大小
 *
 * @param msg [IN] 设置信息，包含本地NUMA、远端NUMA和大小
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_remote_numa_info_set(struct SetRemoteNumaInfoMsg *msg);

/* *
 * @brief   查询进程的页面冷热信息
 *
 * @param pid        [IN] 查询的进程
 * @param data       [IN/OUT] 查询页面冷热数据
 * @param lengthIn   [IN] 传入的数据长度
 * @param lengthOut  [OUT] 查询的返回长度
 * @param dataSource [IN] 标识数据来源
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_freq_query(int pid, uint16_t *data, uint32_t lengthIn, uint32_t *lengthOut, int dataSource);

/* *
 * @brief   设置SMAP运行模式
 *
 * @param runMode [IN] 运行模式，动态和固定比例迁移
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_run_mode_set(int runMode);

/* *
 * @brief   查询SMAP是否运行
 *
 * @return bool  true：运行；false：未运行
 */
bool ubturbo_smap_is_running(void);

/* *
 * @brief   SMAP同步迁出
 *
 * @param msg         [IN] 迁移进程信息，包含迁移进程、远端NUMA、迁移比例、迁移页面大小和迁移模式
 * @param pidType     [IN] 进程类型，目前支持4KB和2MB进程类型
 * @param maxWaitTime [IN] 一次调用最大等待时间，单位ms
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_migrate_out_sync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime);

/* 迁移远端内存相关接口 */

struct MigrateNumaMsg {
    int srcNid;
    int destNid;
    int count;
    uint64_t memids[MAX_NR_MIGNUMA];
};

/* *
 * @brief   添加进程到冷热扫描
 *
 * @param pidArr   [IN] 进程数组
 * @param scanTime [IN] 扫描间隔
 * @param duration [IN] 扫描持续时长
 * @param len      [IN] 进程数组大小
 * @param scanType [IN] 扫描类型
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_process_tracking_add(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType);

/* *
 * @brief   移除进程冷热扫描
 *
 * @param pidArr [IN] 进程数组起始地址
 * @param len    [IN] 进程数组大小
 * @param flag   [IN] 标志位
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_process_tracking_remove(pid_t *pidArr, int len, int flag);

/* *
 * @brief   使能进程的页面迁移
 *
 * @param pidArr [IN] 进程数组起始地址
 * @param len    [IN] 进程数组大小
 * @param enable [IN] 使能开关
 * @param flags  [IN] 标志位
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_process_migrate_enable(pid_t *pidArr, int len, int enable, int flags);

/* *
 * @brief   迁移远端内存到远端内存
 *
 * @param msg [IN] 远端内存信息，包含源NUMA、目标NUMA、物理地址段
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_remote_numa_migrate(struct MigrateNumaMsg *msg);

/* *
 * @brief   迁移远端内存到同远端内存
 *
 * @param msg [IN] 远端内存信息，包含源NUMA、目标NUMA、物理地址段
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_same_remote_numa_migrate(struct MigrateNumaMsg *msg);

/* *
 * @brief   迁移指定进程远端内存到远端内存
 *
 * @param pidArr   [IN] 进程数组起始地址
 * @param len      [IN] 进程数组大小
 * @param srcNid   [IN] 源远端NUMA
 * @param destNid  [IN] 目标远端NUMA
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_pid_remote_numa_migrate(struct MigrateEscapeMsg *msg);

/* *
 * @brief   查询远端设置为nid的进程
 *
 * @param nid      [IN] 远端NUMA ID
 * @param result   [OUT] 结果数组指针
 * @param inLen    [IN] 结果数组大小
 * @param outLen   [OUT] 返回数组大小
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_process_config_query(int nid, struct OldProcessPayload *result, int inLen, int *outLen);

/* *
 * @brief   查询NUMA的频次统计信息
 *
 * @param numa      [IN] 远端NUMA数组起始地址
 * @param freq      [OUT] 结果数组指针
 * @param length    [IN] 数组大小
 * @return int  0：操作成功；非0：操作失败
 */
int ubturbo_smap_remote_numa_freq_query(uint16_t *numa, uint64_t *freq, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* __SMAP_INTERFACE_H__ */
