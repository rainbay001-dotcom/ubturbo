/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef RMRS_SMAP_MODULE_H
#define RMRS_SMAP_MODULE_H

#include <dlfcn.h>
#include <sstream>
#include "rmrs_error.h"

namespace rmrs::smap {
const char * const SMAP_LIBSMAPSO_PATH_RMRS = "/usr/lib64/libsmap.so";
const int RUN_MODE_RMRS = 1;
const int SMAP_RATIO_RMRS = 25;
const int MAX_NR_MIGOUT_RMRS = 40;
const int MAX_NR_MIGBACK_RMRS = 50;
const int MAX_NR_REMOVE_RMRS = 40;

// 迁出接口超时返回码
const int MIGRATEOUT_TIMEOUT_RES = -16;

// 迁移模式
typedef enum {
    MIG_RATIO_MODE = 0, // 按照比例迁移
    MIG_MEMSIZE_MODE,   // 按照内存大小迁移
} MigrateMode;

struct MigrateOutPayload {
    int destNid{};          // remoteNumaId
    pid_t pid{};            // 待迁移冷数据的虚机实例进程pid
    int ratio = SMAP_RATIO_RMRS; // 默认冷数据迁移比例
    uint64_t memSize;        // 新增字段： 内存迁移大小(KB)
    MigrateMode migrateMode; // 新增字段： 内存迁移模式，按照比例或是大小
};

struct MigrateOutMsg {
    int count{};
    MigrateOutPayload payload[MAX_NR_MIGOUT_RMRS];
};

struct RemoteNumaInfo {
    int srcNid;
    int destNid;
    uint64_t size;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"srcNid":)" << srcNid << R"(,)";
        oss << R"("destNid":)" << destNid << R"(,)";
        oss << R"("size":)" << size << R"(})";
        return oss.str();
    }
};

struct RemovePayload {
    pid_t pid;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"("pid":)" << pid;
        return oss.str();
    }
};

struct RemoveMsg {
    int count;
    struct RemovePayload payload[MAX_NR_REMOVE_RMRS];

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"count":)" << count << R"(,)";
        oss << R"("payload":[)";
        for (size_t i = 0; i < count; ++i) {
            if (i != 0) {
                oss << R"(,)";
            }
            oss << payload[i].ToString();
        }
        oss << R"(]})";
        return oss.str();
    };
};

struct MigrateBackPayload {
    int srcNid;
    int destNid;
    uint64_t memid;
};

struct MigrateBackMsg {
    unsigned long long taskID;
    int count;
    struct MigrateBackPayload payload[MAX_NR_MIGBACK_RMRS];
};

struct EnableNodeMsg {
    int enable;
    int nid;
};

struct MigrateNumaPayload {
    // 地址段起始地址
    uint64_t paStart;
    // 地址段结束地址
    uint64_t paEnd;
};

struct MigrateNumaMsg {
    // 源 远端NUMAID
    int srcNid;
    // 目的 远端NUMAID
    int destNid;
    // 1-50的整数  源NUMA地址段数量
    int count;
    // 长度最大为50  地址段信息
    MigrateNumaPayload payload[50];
};

struct SetRemoteNumaInfoMsg {
    int srcNid;
    int destNid;
    uint64_t size;
};

struct ProcessPayload {
    pid_t pid;
    uint8_t ratio; // remote ratio set by upstream component
    uint8_t scanType;
    uint8_t type;
    uint8_t state;
    uint8_t migrateMode;
    int16_t l1Node[4];
    int16_t l2Node[4];
    uint32_t scanTime;
    uint64_t memSize;
};

using SmapInitFunc = int (*)(const uint32_t, void(int, const char *, const char *));
using SmapMigrateOutFunc = int (*)(MigrateOutMsg *, int);
using SmapMigrateOutSyncFunc = int (*)(MigrateOutMsg *, int, uint64_t);
using SmapQueryVmFreqFunc = int (*)(int, uint16_t *, uint16_t, uint16_t&);
using SetSmapRemoteNumaInfoFunc = int (*)(RemoteNumaInfo *);
using SetSmapRunModeFunc = int (*)(int);
using SmapRemoveFunc = int (*)(RemoveMsg *, int);
using SmapEnableNodeFunc = int (*)(EnableNodeMsg *);
using SmapMigratePidRemoteNumaFunc = int (*)(pid_t *, int, int, int);
using SmapEnableProcessMigrateFunc = int (*)(pid_t *, int, int, int);
/* *
 * @brief   查询远端设置为nid的进程
 *
 * @param nid      [IN] 远端NUMA ID
 * @param result   [OUT] 结果数组指针
 * @param inLen    [IN] 结果数组大小
 * @param outLen   [OUT] 返回数组大小
 * @return int  0：操作成功；非0：操作失败
 */
using SmapQueryProcessConfigFunc = int (*)(int, ProcessPayload *, int, int *);

class SmapModule {
public:
    static RmrsResult Init();

    static SmapInitFunc GetSmapInit();

    static SetSmapRemoteNumaInfoFunc GetSetSmapRemoteNumaInfo();

    static SmapMigrateOutFunc GetSmapMigrateOut();

    static SmapMigrateOutSyncFunc GetSmapMigrateOutSync();

    static SmapQueryVmFreqFunc GetSmapQueryVmFreq();

    static void CloseSmapHandle();

    static SetSmapRunModeFunc GetSetSmapRunModeFunc();

    static SmapRemoveFunc GetSmapRemoveFunc();

    static SmapEnableNodeFunc GetSmapEnableNodeFunc();

    static SmapMigratePidRemoteNumaFunc GetSmapMigratePidRemoteNumaFunc();

    static SmapEnableProcessMigrateFunc GetSmapEnableProcessMigrateFunc();

    static SmapQueryProcessConfigFunc GetSmapQueryProcessConfigFunc();

private:
    static void *smapHandle;
    static SmapInitFunc smapInitFunc;
    static SmapMigrateOutFunc smapMigrateOutFunc;
    static SmapMigrateOutSyncFunc smapMigrateOutSyncFunc;
    static SmapQueryVmFreqFunc smapQueryVmFreqFunc;
    static SetSmapRemoteNumaInfoFunc setSmapRemoteNumaInfoFunc;
    static SetSmapRunModeFunc setSmapRunModeFunc;
    static SmapRemoveFunc smapRemoveFunc;
    static SmapEnableNodeFunc smapEnableNodeFunc;
    static SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc;
    static SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc;
    static SmapQueryProcessConfigFunc smapQueryProcessConfigFunc;
};
} // namespace rmrs::smap
#endif // RMRS_SMAP_MODULE_H
