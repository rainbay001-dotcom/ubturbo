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
#include "rmrs_smap_helper.h"

#include <unordered_map>
#include <fstream>
#include <limits>

#include "rmrs_config.h"
#include "rmrs_error.h"
#include "turbo_conf.h"
#include "turbo_logger.h"
#include "rmrs_pointer_process.h"

#define LOG_ERROR UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_DEBUG UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)

namespace rmrs::smap {
constexpr int SMAP_OK = 0;
constexpr int SMAP_ERROR = 1;

using std::lock_guard;
using std::nothrow;
using namespace rmrs;
using namespace rmrs::smap;
using namespace turbo::config;
using namespace turbo::log;
using namespace mempooling;

const int RmrsSmapHelper::smapParamErrorCode = -22;
const int RmrsSmapHelper::smapDealErrorCode = -9;
const int RmrsSmapHelper::smapApplyMemErrorCode = -12;
const int RmrsSmapHelper::smapTimeDoutErrorCode = -110;
const int RmrsSmapHelper::smapPermErrorCode = -1;     // SMAP处理异常错误码 Operation not permitted -1
const int RmrsSmapHelper::smapIOErrorCode = -5;       // SMAP处理异常错误码 I/O error -5
const int RmrsSmapHelper::smapRangeErrorCode = -34;   // SMAP处理异常错误码 Math result not representable -34
const int RmrsSmapHelper::smapBadFNErrorCode = -9;    // SMAP处理异常错误码 Bad file number -9
const int RmrsSmapHelper::smapTimeOutErrorCode = -16; // SMAP处理异常错误码, 迁出接口超时返回码 -16

const int RmrsSmapHelper::enableModeDisableNumaMig = 0;
const int RmrsSmapHelper::enableModeEnableNumaMig = 1;
const int RmrsSmapHelper::smapMigrateBackDefaultDestNid = -1;
const int RmrsSmapHelper::paStartEndRangeSize = 2;
const uint32_t PAGE_TYPE_RMRS = 1;
const int PID_TYPE_2MB = 1;
const int RUN_MODE_VM = 0;
const int RUN_MODE_MP = 1;
const int SMAP_QUERY_PID_NUM = 40;

mutex RmrsSmapHelper::gMutex;

void RmrsSmapHelper::RackVmLog(int level, const char *str, const char *moduleName)
{
    if (moduleName == nullptr) {
        moduleName = RMRS_MODULE_NAME;
    }

    if (str == nullptr) {
        str = "(null)";
    }

    switch (level) {
        case static_cast<uint32_t>(RmrsLogLevel::DEBUG):
            UBTURBO_LOG_DEBUG(moduleName, RMRS_MODULE_CODE) << str;
            break;
        case static_cast<uint32_t>(RmrsLogLevel::INFO):
            UBTURBO_LOG_INFO(moduleName, RMRS_MODULE_CODE) << str;
            break;
        case static_cast<uint32_t>(RmrsLogLevel::WARN):
            UBTURBO_LOG_WARN(moduleName, RMRS_MODULE_CODE) << str;
            break;
        case static_cast<uint32_t>(RmrsLogLevel::ERROR):
        default:
            UBTURBO_LOG_ERROR(moduleName, RMRS_MODULE_CODE) << str;
    }
}

RmrsResult RmrsSmapHelper::Init()
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Init start.";

    RmrsResult ret = SmapModule::Init();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to init smapModule.";
        return RMRS_ERROR;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapModule init success.";

    return RMRS_OK;
}

void RmrsSmapHelper::Deinit()
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Deinit start.";
    SmapModule::CloseSmapHandle();
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapModule Deinit success.";
}

RmrsResult RmrsSmapHelper::MigrateColdDataToRemoteNuma(std::vector<uint16_t> &remoteNumaIdsIn,
                                                       std::vector<pid_t> &pidsIn, int ratio)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateColdDataToRemoteNuma start.";
    if (pidsIn.size() == 0) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Migrate pid sizes is 0.";
    }
    if (pidsIn.size() != 0) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Migrate pid sizes is " << pidsIn.size() << ".";
        for (size_t i = 0; i < pidsIn.size(); i++) {
            UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsSmapHelper] Migrate pid = " << pidsIn[i] << ".";
        }
    }
    SmapMigrateOutFunc smapMigrateOutFunc = SmapModule::GetSmapMigrateOut();
    if (smapMigrateOutFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }

    MigrateOutMsg migrateOutMsg{};
    RmrsResult retGet = GetMigrateOutMsg(migrateOutMsg, pidsIn, remoteNumaIdsIn, ratio);
    if (retGet != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to fill migrate-out value.";
        return RMRS_ERROR;
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Migrate cold data.";
    int ret = smapMigrateOutFunc(&migrateOutMsg, PID_TYPE_2MB);
    if (ret < 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] MigrateColdDataToRemoteNuma failed, with error code = " << ret << ".";
        if (ret == smapPermErrorCode) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsSmapHelper] MigrateColdDataToRemoteNuma init failed.";
        }
        return RMRS_ERROR;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateColdDataToRemoteNuma end.";
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::QueryVMFreqArray(int pidIn, uint16_t *dataIn, size_t lengthIn, uint16_t &lengthOut)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] QueryVMFreqArray start.";
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] QueryVMFreqArray pid = " << pidIn << ".";
    SmapQueryVmFreqFunc smapQueryVmFreqFunc = SmapModule::GetSmapQueryVmFreq();
    if (smapQueryVmFreqFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Query VM mem freq array.";
    int ret = smapQueryVmFreqFunc(pidIn, dataIn, lengthIn, lengthOut);
    if (ret == SMAP_OK) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Query success.";
    } else if (ret < 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Query freq failed, with error code = " << ret << ".";
        if (ret == smapPermErrorCode) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Init failed.";
        }
        if (ret == smapParamErrorCode) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Param error.";
        }
        return RMRS_ERROR;
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] QueryVMFreqArray end.";
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::SmapMode(int runMode)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SetSmapRunMode start.";
    SetSmapRunModeFunc setSmapRunModeFunc = SmapModule::GetSetSmapRunModeFunc();
    if (setSmapRunModeFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }
    int ret = setSmapRunModeFunc(runMode);
    if (ret == SMAP_OK) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] setSmapRunMode success.";
    } else if (ret < 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Failed to setSmapRunMode, with error code = " << ret << ".";
        if (ret == smapPermErrorCode) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to init setSmapRunMode.";
        }
        if (ret == smapParamErrorCode) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Param error.";
        }
        return RMRS_ERROR;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SetSmapRunMode end.";
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::RewriteHugePages(const std::string &realPath, uint64_t originalHugePages,
                                            uint64_t borrowSize)
{
    // 按2M为计算单位,计算最后分配大页的大小
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[HugeAlloc] Param originalHugePages: " << originalHugePages << ".";
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[HugeAlloc] Param borrowSize: " << borrowSize << ".";
    uint64_t nrHugePages = borrowSize / (2 * 1024 * 1024) + originalHugePages;

    char *canonicalPath = realpath(realPath.c_str(), nullptr);
    if (canonicalPath == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[HugeAlloc] Failed to get canonicalPath, realPath: " << realPath << ".";
        return RMRS_ERROR;
    }

    std::ofstream outputFile(canonicalPath, std::ios::out);
    free(canonicalPath);
    if (!outputFile.is_open()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[HugeAlloc] Failed to open file, realPath: " << realPath << ".";
        return RMRS_ERROR;
    }
    outputFile << nrHugePages << std::endl;
    outputFile.close();
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[HugeAlloc] Allocate HugePages: " << nrHugePages << ".";
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::GetOriginalHugePages(const std::string &realPath, uint64_t &originalHugePages)
{
    char *canonicalPath = realpath(realPath.c_str(), nullptr);
    if (canonicalPath == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[HugeAlloc] Failed to get canonicalPath, realPath: " << realPath << ".";
        return RMRS_ERROR;
    }

    std::ifstream inputFile(canonicalPath);
    free(canonicalPath);
    if (!inputFile.is_open()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[HugeAlloc] Failed to open inputFile, realPath: " << realPath << ".";
        return RMRS_ERROR;
    }
    std::string line;
    std::getline(inputFile, line);
    inputFile.close();
    std::istringstream iss(line);
    if (!(iss >> originalHugePages)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[HugeAlloc] Failed to parse integer from file.";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::GetMigrateOutMsg(MigrateOutMsg &migrateOutMsg, std::vector<pid_t> &pidsIn,
                                            std::vector<uint16_t> &remoteNumaIdsIn, int &ratio)
{
    if (pidsIn.size() > MAX_NR_MIGOUT_RMRS) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Migrate-out PID array size too large.";
        return RMRS_ERROR;
    }

    if (remoteNumaIdsIn.size() > MAX_NR_MIGOUT_RMRS) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Migrate-out remote NUMAId array size too large.";
        return RMRS_ERROR;
    }

    if (remoteNumaIdsIn.size() >= pidsIn.size()) {
        migrateOutMsg.count = static_cast<int>(pidsIn.size());
        for (int i = 0; i < migrateOutMsg.count; i++) {
            migrateOutMsg.payload[i].destNid = static_cast<int>(remoteNumaIdsIn[i]);
            migrateOutMsg.payload[i].pid = static_cast<int>(pidsIn[i]);
            migrateOutMsg.payload[i].ratio = ratio;
            UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateOutMsg."
                                                                << "pid: " << migrateOutMsg.payload[i].pid << ".";
        }
    } else {
        migrateOutMsg.count = static_cast<int>(pidsIn.size());
        for (int i = 0; i < migrateOutMsg.count; i++) {
            migrateOutMsg.payload[i].destNid =
                static_cast<int>(remoteNumaIdsIn[i % static_cast<int>(remoteNumaIdsIn.size())]);
            migrateOutMsg.payload[i].pid = static_cast<int>(pidsIn[i]);
            migrateOutMsg.payload[i].ratio = ratio;
        }
    }
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(std::vector<pid_t> &vmPids)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapRemoveVMPidToRemoteNuma start.";
    SmapRemoveFunc smapRemoveFunc = SmapModule::GetSmapRemoveFunc();
    if (smapRemoveFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }

    RemoveMsg msg;
    msg.count = static_cast<int>(vmPids.size());

    if (msg.count == 0) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] No VM need to remove, size = " << msg.count << ".";
        return RMRS_OK;
    }

    if (msg.count > MAX_NR_REMOVE_RMRS) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Too many pids to remove, size = " << msg.count << ".";
        return RMRS_ERROR;
    }

    for (int i = 0; i < msg.count; i++) {
        msg.payload[i].pid = vmPids[i];
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapRemoveVMPidToRemoteNuma load pid = " << msg.payload[i].pid << ".";
    }

    int ret = smapRemoveFunc(&msg, PID_TYPE_2MB);
    if (ret == SMAP_OK) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapRemoveVMPidToRemoteNuma success.";
    } else if (ret == smapParamErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapRemove param error, ret = " << ret << ".";
        return RMRS_ERROR;
    } else if (ret == smapDealErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapRemove deal error, ret = " << ret << ".";
        return RMRS_ERROR;
    } else {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapRemove error, ret = " << ret << ".";
        return RMRS_ERROR;
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapRemoveVMPidToRemoteNuma end.";
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::SmapEnableRemoteNuma(int remoteNumaId)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapEnableRemoteNuma start.";
    SmapEnableNodeFunc smapEnableNodeFunc = SmapModule::GetSmapEnableNodeFunc();
    if (smapEnableNodeFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }

    EnableNodeMsg msg;
    msg.enable = enableModeEnableNumaMig;
    msg.nid = remoteNumaId;
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsSmapHelper] Call smapEnableNodeFunc, msg.nid[" << msg.nid << "] msg.enable[" << msg.enable << "].";

    int ret = smapEnableNodeFunc(&msg);
    if (ret == SMAP_OK) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapEnableRemoteNuma success.";
    } else if (ret == smapParamErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapEnableRemoteNuma param error. ret: " << ret << ".";
        return RMRS_ERROR;
    } else {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapEnableRemoteNuma error. ret: " << ret << ".";
        return RMRS_ERROR;
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapEnableRemoteNuma end.";
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::SmapMigratePidRemoteNumaHelper(pid_t *pidArr, int len, int srcNid, int destNid)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapMigratePidRemoteNumaHelper start.";
    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = SmapModule::GetSmapMigratePidRemoteNumaFunc();
    if (smapMigratePidRemoteNumaFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Ptr smapMigratePidRemoteNumaFunc == nullptr.";
        return RMRS_ERROR;
    }

    int ret = smapMigratePidRemoteNumaFunc(pidArr, len, srcNid, destNid);
    if (ret == SMAP_OK) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapMigratePidRemoteNumaHelper success.";
    } else if (ret == smapPermErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Operation not permitted. ret: " << ret << ".";
        return RMRS_ERROR;
    } else if (ret == smapParamErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Invalid argument. ret: " << ret << ".";
        return RMRS_ERROR;
    } else if (ret == smapTimeDoutErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Connection timed out. ret: " << ret << ".";
        return RMRS_ERROR;
    } else if (ret == smapIOErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] I/O error. ret: " << ret << ".";
        return RMRS_ERROR;
    } else {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapMigratePidRemoteNumaHelper error. ret: " << ret << ".";
        return RMRS_ERROR;
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapMigratePidRemoteNumaHelper end.";
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::SmapEnableProcessMigrateHelper(pid_t *pidArr, int len, int enable, int flags)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapEnableProcessMigrateHelper start.";
    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = SmapModule::GetSmapEnableProcessMigrateFunc();
    if (smapEnableProcessMigrateFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }

    int ret = smapEnableProcessMigrateFunc(pidArr, len, enable, flags);
    if (ret == SMAP_OK) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapEnableProcessMigrateHelper success.";
    } else if (ret == smapPermErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Operation not permitted. ret: " << ret << ".";
        return RMRS_ERROR;
    } else if (ret == smapParamErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Invalid argument. ret: " << ret << ".";
        return RMRS_ERROR;
    } else if (ret == smapApplyMemErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Out of memory. ret: " << ret << ".";
        return RMRS_ERROR;
    } else if (ret == smapRangeErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] The smapRangeErrorCode. ret: " << ret << ".";
        return RMRS_ERROR;
    } else if (ret == smapBadFNErrorCode) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Bad file number. ret: " << ret << ".";
        return RMRS_ERROR;
    } else {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] SmapEnableProcessMigrateHelper error. ret: " << ret << ".";
        return RMRS_ERROR;
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapEnableProcessMigrateHelper end.";
    return RMRS_OK;
}

RmrsResult RmrsSmapHelper::SetSmapRemoteNumaInfoHelper(
    const uint16_t &srcNumaId, const std::vector<mempooling::over_commit::MemBorrowInfo> &memBorrowInfos)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SetSmapRemoteNumaInfo start";
    const SetSmapRemoteNumaInfoFunc setSmapRemoteNumaInfo = SmapModule::GetSetSmapRemoteNumaInfo();
    if (setSmapRemoteNumaInfo == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }

    for (const auto &[presentNumaId, borrowSize] : memBorrowInfos) {
        RemoteNumaInfo remoteNumaInfo = {
            .srcNid = srcNumaId, .destNid = presentNumaId, .size = borrowSize >> mempooling::over_commit::KB2MB};
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] remoteNumaInfo: " << remoteNumaInfo.ToString() << ".";
        int ret = setSmapRemoteNumaInfo(&remoteNumaInfo);
        if (ret == SMAP_OK) {
            UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SetSmapRemoteNumaInfo success.";
        } else if (ret == smapPermErrorCode) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsSmapHelper] SetSmapRemoteNumaInfo faild. ret: " << ret << ".";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

MigrateOutMsg RmrsSmapHelper::GetMigrateOutMsgInOverCommit(
    const std::vector<mempooling::over_commit::MemMigrateResult> &memMigrateResults, const uint16_t ratio)
{
    MigrateOutMsg migrateOutMsg{};
    migrateOutMsg.count = static_cast<int>(memMigrateResults.size());
    if (migrateOutMsg.count > MAX_NR_MIGOUT_RMRS) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Parameter size overflow.";
        return migrateOutMsg;
    }
    for (int i = 0; i < migrateOutMsg.count; i++) {
        migrateOutMsg.payload[i].pid = memMigrateResults[i].pid;
        migrateOutMsg.payload[i].destNid = memMigrateResults[i].remoteNumaId;
        migrateOutMsg.payload[i].ratio = ratio;
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Pid=" << memMigrateResults[i].pid
            << ", destNid=" << memMigrateResults[i].remoteNumaId << ", ratio=" << ratio << ".";
    }
    return migrateOutMsg;
}

RmrsResult RmrsSmapHelper::MigrateOutInOverCommit(const std::vector<over_commit::MemMigrateResult> &memMigrateResults,
                                                  const uint16_t ratio)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateOutInOverCommit start.";
    const SmapMigrateOutFunc smapMigrateOutFunc = SmapModule::GetSmapMigrateOut();
    if (smapMigrateOutFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    };

    auto migrateOutMsg = GetMigrateOutMsgInOverCommit(memMigrateResults, ratio);
    if (const auto ret = smapMigrateOutFunc(&migrateOutMsg, PID_TYPE_2MB); ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateOutInOverCommit end.";
    }
    return RMRS_OK;
}

// 内存迁出同步接口 迁出具体size大小
RmrsResult RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(std::vector<uint16_t> &remoteNumaIdList,
                                                           std::vector<pid_t> &pidsIn,
                                                           std::vector<uint64_t> memSizeList, uint64_t waitTime)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateColdDataToRemoteNumaSync start.";
    if (pidsIn.size() == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateSync pid sizes = 0.";
        return RMRS_ERROR;
    }
    if (pidsIn.size() != 0) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] MigrateSync pid sizes=" << pidsIn.size() << ".";
        for (size_t i = 0; i < pidsIn.size(); i++) {
            UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsSmapHelper] MigrateSync pid=" << pidsIn[i] << ".";
        }
    }

    SmapMigrateOutSyncFunc smapMigrateOutSyncFunc = SmapModule::GetSmapMigrateOutSync();
    if (smapMigrateOutSyncFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }
    MigrateOutMsg migrateOutMsg{};
    if (!GetMigrateOutMsgByMemSize(migrateOutMsg, pidsIn, remoteNumaIdList, memSizeList)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] GetMigrateOutMsgByMemSize failed.";
        return RMRS_ERROR;
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateSync cold data.";
    int ret = smapMigrateOutSyncFunc(&migrateOutMsg, PID_TYPE_2MB, waitTime);
    if (ret < 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] MigrateColdDataToRemoteNumaSync failed" << ret << ".";
        if (ret == smapPermErrorCode) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsSmapHelper] MigrateColdDataToRemoteNumaSync Init failed.";
        }
        if (ret == smapTimeOutErrorCode) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsSmapHelper] MigrateColdDataToRemoteNumaSync timeout.";
        }
        return RMRS_ERROR;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] MigrateColdDataToRemoteNumaSync end.";
    return RMRS_OK;
}

bool RmrsSmapHelper::GetMigrateOutMsgByMemSize(MigrateOutMsg &migrateOutMsg, std::vector<pid_t> pidList,
                                               std::vector<uint16_t> remoteNumaIdList,
                                               std::vector<uint64_t> memSizeList)
{
    if (pidList.empty() || memSizeList.empty() || remoteNumaIdList.empty() || pidList.size() != memSizeList.size() ||
        pidList.size() != remoteNumaIdList.size()) {
        LOG_ERROR << "[RmrsSmapHelper] Input lists are empty or sizes are not equal.";
        return false;
    }
    migrateOutMsg.count = static_cast<int>(pidList.size());
    if (migrateOutMsg.count > MAX_NR_MIGOUT_RMRS) {
        LOG_ERROR << "[RmrsSmapHelper] Parameter limit exceeded.";
        return false;
    }
    for (size_t i = 0; i < pidList.size(); i++) {
        migrateOutMsg.payload[i].destNid = static_cast<int>(remoteNumaIdList[i]);
        migrateOutMsg.payload[i].pid = pidList[i];
        migrateOutMsg.payload[i].ratio = 0;
        migrateOutMsg.payload[i].memSize = memSizeList[i];
        migrateOutMsg.payload[i].migrateMode = MIG_MEMSIZE_MODE;
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Index[" << i << "].";
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] DestNid[" << migrateOutMsg.payload[i].destNid << "].";
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] PID[" << migrateOutMsg.payload[i].pid << "].";
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] Ratio[" << migrateOutMsg.payload[i].ratio << "].";
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapHelper] MemSize[" << migrateOutMsg.payload[i].memSize << "].";
    }
    return true;
}

RmrsResult RmrsSmapHelper::SmapQueryProcessConfigHelper(int nid, std::vector<ProcessPayload> &processPayloadList)
{
    const SmapQueryProcessConfigFunc smapQueryProcessConfigFunc = SmapModule::GetSmapQueryProcessConfigFunc();
    if (smapQueryProcessConfigFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] Failed to get function symbol.";
        return RMRS_ERROR;
    }

    ProcessPayload payloadArr[SMAP_QUERY_PID_NUM];
    int realLen = 0;
    int res = smapQueryProcessConfigFunc(nid, payloadArr, SMAP_QUERY_PID_NUM, &realLen);
    if (res != SMAP_OK || realLen < 0 || realLen > SMAP_QUERY_PID_NUM) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapHelper] SmapQueryProcessConfig error."
            << nid << " "<< realLen << " " << res;
        return RMRS_ERROR;
    }
    for (int i = 0; i < realLen; i++) {
        processPayloadList.push_back(payloadArr[i]);
    }
    return RMRS_OK;
}

} // namespace rmrs::smap