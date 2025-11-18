/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */#ifndef __RMRS_SMAP_INTERFACE_H__
#define __RMRS_SMAP_INTERFACE_H__

#include <cstdint>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NR_MIGOUT 40
#define MAX_NR_MIGBACK 50
#define MAX_NR_MIGNUMA 50
#define MAX_NR_REMOVE MAX_NR_MIGOUT
#define MAX_NR_TRACKING MAX_NR_MIGOUT
#define ADAPT_ALLOC_PERIOD 1000
#define SCAN_MIGRATE_PERIOD LIGHT_STABLE_MIGRATE_CYCLE
#define USER_DEFAULT_MIGRATE_OUT_RATIO 25
#define MAX_NR_OF_QUERY_VM_FREQ_HCCS 65536
#define WAIT_TIME (100)
#define MAX_WAIT_TIME (60 * 1000)
#define MIN_WAIT_TIME (10 * 1000)
#define MAX_SCAN_TIME 200
#define MIN_SCAN_TIME 5
#define DEFAULT_L2_NODE (-1)

typedef enum {
    INPUT_PROCESS = 0,
    INPUT_VM,
    INPUT_MAX
} InputPidType;

// 迁移模式
typedef enum {
    MIG_RATIO_MODE = 0, // 按照比例迁移
    MIG_MEMSIZE_MODE,   // 按照内存大小迁移
} MigrateMode;

struct MigrateOutPayload {
    int destNid;
    pid_t pid;
    int ratio;
    uint64_t memSize;        // 新增字段： 内存迁移大小(KB)
    MigrateMode migrateMode; // 新增字段： 内存迁移模式，按照比例或是大小
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
    int srcNid;
    int destNid;
    uint64_t size;
};

struct NumaPayload {
    uint8_t local; // L1 NUMA ID，0-255
    uint8_t remote; // L2 NUMA ID，0-255
    uint32_t size; // Available size, unit is MB
};

typedef void (*Logfunc)(int level, const char *str, const char *moduleName);

/* *
 * @brief   设置进程迁移到远端NUMA
 *
 * @param msg      [IN] 迁移进程信息，包含迁移进程、远端NUMA和迁移比例
 * @param pidType  [IN] 进程类型，目前支持4KB和2MB进程类型
 * @return int  0：操作成功；非0：操作失败
 */
int SmapMigrateOut(struct MigrateOutMsg *msg, int pidType);

/* *
 * @brief   迁移指定地址段的远端NUMA内存
 *
 * @param msg   [IN] 迁移地址段信息，包含源NUMA、目标NUMA、地址段
 * @return int  0：操作成功；非0：操作失败
 */
int SmapMigrateBack(struct MigrateBackMsg *msg);

/* *
 * @brief   移除进程的冷热页迁移
 *
 * @param msg   [IN] 移除的进程信息，包含进程的PID
 * @param pidType  [IN] 进程类型，目前支持4KB和2MB进程类型
 * @return int  0：操作成功；非0：操作失败
 */
int SmapRemove(struct RemoveMsg *msg, int pidType);

/* *
 * @brief   使能NUMA的冷热页迁移
 *
 * @param msg   [IN] NUMA信息，包含NUMA ID、使能开关
 * @return int  0：操作成功；非0：操作失败
 */
int SmapEnableNode(struct EnableNodeMsg *msg);

/* *
 * @brief   初始化SMAP模块
 *
 * @param pageType [IN] 页面类型，当前只支持4KB和2MB页
 * @param extlog   [IN] 日志打印函数
 * @return int  0：操作成功；非0：操作失败
 */
int SmapInit(uint32_t pageType, Logfunc extlog);

/* *
 * @brief  卸载SMAP模块
 *
 * @return int  0：操作成功；非0：操作失败
 */
int SmapStop(void);

/* *
 * @brief 紧急迁移本地内存到远端，在发生本地内存无资源场景
 *
 * @param size [IN] 需要紧急迁移的大小
 */
void SmapUrgentMigrateOut(uint64_t size);

/* *
 * @brief   添加进程白名单
 *
 * @param pidName [IN] 进程名称
 * @return int  0：操作成功；非0：操作失败
 */
int SmapBlack(const char *pidName);

/* *
 * @brief   移除进程白名单
 *
 * @param pidName [IN] 进程名称
 * @return int  0：操作成功；非0：操作失败
 */
int SmapUnblack(const char *pidName);

/* *
 * @brief   设置本地NUMA对远端NUMA迁移内存大小
 *
 * @param msg [IN] 设置信息，包含本地NUMA、远端NUMA和大小
 * @return int  0：操作成功；非0：操作失败
 */
int SetSmapRemoteNumaInfo(struct SetRemoteNumaInfoMsg *msg);

/* *
 * @brief   查询进程的页面冷热信息
 *
 * @param pid [IN]        查询的进程
 * @param pid [IN/OUT]    查询页面冷热数据
 * @param lengthIn [IN]   传入的数据长度
 * @param lengthOut [out] 查询的返回长度
 * @return int  0：操作成功；非0：操作失败
 */
int SmapQueryVmFreq(int pid, uint16_t *data, uint16_t lengthIn, uint16_t *lengthOut);

/* *
 * @brief   设置SMAP运行模式
 *
 * @param runMode [IN] 运行模式，动态和固定比例迁移
 * @return int  0：操作成功；非0：操作失败
 */
int SetSmapRunMode(int runMode);

/* *
 * @brief   查询SMAP是否运行
 *
 * @return bool  true：运行；false：未运行
 */
bool SmapIsRunning();

/* *
 * @brief   SMAP同步迁出
 *
 * @param msg      [IN] 迁移进程信息，包含迁移进程、远端NUMA和迁移比例
 * @param pidType  [IN] 进程类型，目前支持4KB和2MB进程类型
 * @param maxWaitTime [IN] 一次调用最大等待时间，单位ms
 * @return int  0：操作成功；非0：操作失败
 */
int SmapMigrateOutSync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime);

/* 迁移远端内存相关接口 */
struct MigrateNumaPayload {
    uint64_t paStart;
    uint64_t paEnd;
};

struct MigrateNumaMsg {
    int srcNid;
    int destNid;
    int count;
    struct MigrateNumaPayload payload[MAX_NR_MIGNUMA];
};

/* *
 * @brief   添加进程到冷热扫描
 *
 * @param pidArr    [IN] 进程数组起始地址
 * @param len       [IN] pid对应扫描时间起始地址
 * @param enable    [IN] 进程数组大小
 * @param scanType  [IN] 扫描类型
 * @return int  0：操作成功；非0：操作失败
 */
int SmapAddProcessTracking(pid_t *pidArr, uint32_t *scanTime, int len, int scanType);

/* *
 * @brief   移除进程冷热扫描
 *
 * @param pidArr [IN] 进程数组起始地址
 * @param len    [IN] 进程数组大小
 * @param flags  [IN] 标志位
 * @return int  0：操作成功；非0：操作失败
 */
int SmapRemoveProcessTracking(pid_t *pidArr, int len, int flag);

/* *
 * @brief   使能进程的页面迁移
 *
 * @param pidArr [IN] 进程数组起始地址
 * @param len    [IN] 进程数组大小
 * @param enable [IN] 使能开关
 * @param flags  [IN] 标志位
 * @return int  0：操作成功；非0：操作失败
 */
int SmapEnableProcessMigrate(pid_t *pidArr, int len, int enable, int flags);

/* *
 * @brief   迁移远端内存到远端内存
 *
 * @param msg [IN] 远端内存信息，包含源NUMA、目标NUMA、物理地址段
 * @return int  0：操作成功；非0：操作失败
 */
int SmapMigrateRemoteNuma(struct MigrateNumaMsg *msg);

/* *
 * @brief   迁移指定进程远端内存到远端内存
 *
 * @param pidArr   [IN] 进程数组起始地址
 * @param len      [IN] 进程数组大小
 * @param srcNid   [IN] 源远端NUMA
 * @param destNid  [IN] 目标远端NUMA
 * @return int  0：操作成功；非0：操作失败
 */
int SmapMigratePidRemoteNuma(pid_t *pidArr, int len, int srcNid, int destNid);

#ifdef __cplusplus
}
#endif

#endif /* __RMRS_SMAP_INTERFACE_H__ */