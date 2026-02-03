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
#ifndef RMRS_SMAP_HELPER_H
#define RMRS_SMAP_HELPER_H

#include <mutex>
#include <string>
#include <vector>
#include <csignal>
#include "turbo_over_commit_def.h"
#include "rmrs_error.h"
#include "rmrs_smap_module.h"
#include "rmrs_file_util.h"

namespace rmrs::smap {

using namespace mempooling;
using std::mutex;

const int SMAP_RATIO = 25;

enum class RmrsLogLevel : uint32_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    CRIT = 4
};

class RmrsSmapHelper {
public:
    static mutex gMutex;

    static RmrsResult Init();

    static void Deinit();

    static bool GetMigrateOutMsgByMemSize(MigrateOutMsg &migrateOutMsg, std::vector<pid_t> pidList,
                                          std::vector<uint16_t> remoteNumaIdList, std::vector<uint64_t> memSizeList);

    static RmrsResult GetOriginalHugePages(const std::string &filePath, uint64_t &originalHugePages);

    static RmrsResult RewriteHugePages(const std::string &realPath, uint64_t originalHugePages, uint64_t borrowSize);

    static RmrsResult MigrateColdDataToRemoteNumaSync(std::vector<uint16_t> &remoteNumaIdsIn,
                                                      std::vector<pid_t> &pidsIn, std::vector<uint64_t> memSizeList,
                                                      uint64_t waitTime);

    static RmrsResult QueryVMFreqArray(int pidIn, uint16_t *dataIn, uint32_t lengthIn, uint32_t &lengthOut);

    static RmrsResult SmapMode(int runMode);

    static RmrsResult SmapRemoveVMPidToRemoteNuma(std::vector<pid_t> &vmPids);

    static RmrsResult SmapEnableRemoteNuma(int remoteNumaId);

    static RmrsResult SmapMigratePidRemoteNumaHelper(pid_t *pidArr, int len, int srcNid, int destNid);

    static RmrsResult SmapEnableProcessMigrateHelper(pid_t *pidArr, int len, int enable, int flags);

    // 避免和SetSmapRemoteNumaInfo重名添加Helper后缀
    static RmrsResult SetSmapRemoteNumaInfoHelper(
        const uint16_t &srcNumaId, const std::vector<mempooling::over_commit::MemBorrowInfo> &memBorrowInfos);

    static RmrsResult SmapQueryProcessConfigHelper(int nid, std::vector<ProcessPayload> &processPayloadList);

    static const int smapParamErrorCode;    // SMAP参数错误码 Invalid argument -22
    static const int smapDealErrorCode;     // SMAP处理异常错误码 -9
    static const int smapApplyMemErrorCode; // SMAP处理异常错误码 -12
    static const int smapTimeDoutErrorCode; // SMAP处理异常错误码 Connection timed out -110
    static const int smapPermErrorCode;     // SMAP处理异常错误码 Operation not permitted -1
    static const int smapIOErrorCode;       // SMAP处理异常错误码 I/O error -5
    static const int smapRangeErrorCode;    // SMAP处理异常错误码 Math result not representable -34
    static const int smapBadFNErrorCode;    // SMAP处理异常错误码 Bad file number -9
    static const int smapTimeOutErrorCode;  // SMAP处理异常错误码 迁出超时 -16
    static const int smapVmDeleteErrorCode; // SMAP处理异常错误码, 迁出过程中pid被删除 -3

    static const int smapMigrateBackDefaultDestNid; // smapMigrateBack destNid 默认参数设置为 -1
    static const int enableModeDisableNumaMig;      // 关闭远端nume冷热流动标识符 0
    static const int enableModeEnableNumaMig;       // 开启远端numa冷热流动标识符 1
    static const int paStartEndRangeSize;           // smapMigrateBack paStartEnd 包含数组大小 2

private:
    const std::string HUGEPAGES_PATH_HEAD = "/sys/devices/system/node/node";
    const std::string HUGEPAGES_PATH_TAIL = "/hugepages/hugepages-2048kB/nr_hugepages";
    const std::string FD_SMAP_VM_DEVICE = "/dev/smap_vm_device";

    static void RackVmLog(int level, const char *str, const char *moduleName);
};
} // namespace rmrs::smap

#endif // RMRS_SMAP_HELPER_H
