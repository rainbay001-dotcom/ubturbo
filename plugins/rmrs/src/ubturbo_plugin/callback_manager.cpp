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
#include "callback_manager.h"

#include "turbo_resource_query.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <fstream>
#include <sys/stat.h>

#include "rmrs_string_util.h"
#include "libvirt_helper/rmrs_libvirt_helper.h"
#include "rmrs_config.h"
#include "rmrs_json_util.h"
#include "rmrs_rollback_module.h"
#include "rmrs_memfree_module.h"
#include "rmrs_resource_export.h"
#include "rmrs_serializer.h"
#include "rmrs_smap_helper.h"
#include "turbo_conf.h"
#include "turbo_def.h"
#include "turbo_ipc_server.h"
#include "turbo_logger.h"
#include "turbo_ucache_message.h"
#include "ucache_migration_executor.h"
#include "bottleneck_detector.h"

namespace rmrs {
#define LOG_DEBUG UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_ERROR UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_INFO UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)

using namespace turbo::ipc::server;
using namespace rmrs::migrate;
using namespace rmrs::serialization;
using namespace rmrs::migrate;
using namespace rmrs::smap;
using namespace turbo::config;
using namespace rmrs::exports;

const int CallbackManager::memFragmentationMode = 1; // smapRunmode设置为碎片场景--参数为1

bool CallbackManager::isSetSmapMode = false;

RmrsResult CallbackManager::Init()
{
    RmrsResult ret = UBTurboRegIpcService("BorrowRollbackRecvHandler", CallbackManager::BorrowRollbackRecvHandler);
    ret |= UBTurboRegIpcService("MigrateStrategyRecvHandler", CallbackManager::MigrateStrategyRecvHandler);
    ret |= UBTurboRegIpcService("MigrateBackRecvHandler", CallbackManager::MigrateBackRecvHandler);
    ret |= UBTurboRegIpcService("MigrateExecuteRecvHandler", CallbackManager::MigrateExecuteRecvHandler);
    ret |= UBTurboRegIpcService("HeartBeatRecvHandler", CallbackManager::HeartBeatRecvHandler);
    ret |= UBTurboRegIpcService("PidNumaInfoCollectRecvHandler", CallbackManager::PidNumaInfoCollectRecvHandler);
    ret |= UBTurboRegIpcService("NumaMemInfoCollectRecvHandler", CallbackManager::NumaMemInfoCollectRecvHandler);
    ret |= UBTurboRegIpcService("UCacheMigrateStrategyRecvHandler", CallbackManager::UCacheMigrateStrategyRecvHandler);
    ret |= UBTurboRegIpcService("UCacheMigrateStopRecvHandler", CallbackManager::UCacheMigrateStopRecvHandler);
    ret |= UBTurboRegIpcService("UpdateUCacheRatioRecvHandler", CallbackManager::UpdateUCacheRatioRecvHandler);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[CallbackManager] UBTurboRegIpcService failed.";
        return RMRS_ERROR;
    }

    ret = ResourceExport::Init();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[CallbackManager] Resource export init failed" << ret;
        return RMRS_ERROR;
    }
    ret = LibvirtHelper::Init();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[CallbackManager] Failed to init libvirt helper.";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

RmrsResult CallbackManager::SetResponse(RMRSResponseInfoSimpo &response, const RmrsResult &retCode,
                                        const std::string &msg, TurboByteBuffer &resBuffer)
{
    response.SetResponseInfo(retCode, msg);
    RmrsOutStream builder;
    builder << response;

    resBuffer.data = builder.GetBufferPointer();
    resBuffer.len = builder.GetSize();
    resBuffer.freeFunc = DefaultFreeFunc;
    return RMRS_OK;
}

RmrsResult CallbackManager::HeartBeatRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    resp.len = 1;
    resp.data = new uint8_t[resp.len];
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    resp.data[0] = 0;
    return RMRS_OK;
}

RmrsResult CallbackManager::BorrowRollbackRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[CallbackManager] MemBorrowRollbackRecvHandler start.";

    if (req.data == nullptr || req.len == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[CallbackManager] Request data is null, len= << req.len <<.";
        return RMRS_ERROR;
    }

    RmrsResult retSmap = CallbackManager::InitSmap();
    if (retSmap != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[CallbackManager] InitSmap failed, retSmap: " << retSmap << ".";
        return RMRS_ERROR;
    }
    auto res = RmrsRollbackModule::HandlerRollback(req, resp);
    if (RMRS_OK != res) {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[CallbackManager] Recv MigrateStrategyRmrs res: " << res << ".";
        return RMRS_ERROR;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[CallbackManager] MemBorrowRollbackRecvHandler end.";
    return RMRS_OK;
}

static const std::string MIGREATE_RECORD_PATH = "/opt/ubturbo/";
static const std::string MIGREATE_RECORD_FILE = "/opt/ubturbo/migrate_record";

RmrsResult CallbackManager::MigrateExecuteRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    if (req.data == nullptr || req.len == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemMigrate][MemMigrate] Request data is null, len= << req.len <<.";
        return RMRS_ERROR;
    }

    RmrsResult retSmap = CallbackManager::InitSmap();
    if (retSmap != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemMigrate][MemMigrate] InitSmap failed, retSmap: " << retSmap << ".";
        return RMRS_ERROR;
    }

    rmrs::serialization::MigrateStrategyResult migrateStrategyResult;
    RmrsInStream builder(req.data, req.len);
    builder >> migrateStrategyResult;

    for (auto &vm : migrateStrategyResult.vmInfoList) {
        if (vm.pid < 0) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[MemMigrate][MemMigrate] Pid is invalid.";
            return RMRS_ERROR;
        }
    }

    std::ofstream fout;
    if (std::filesystem::exists(MIGREATE_RECORD_PATH)) {
        fout.open(MIGREATE_RECORD_FILE, std::ios::out | std::ios::binary);
        std::string text(reinterpret_cast<char *>(req.data), req.len);
        fout << text;
        fout.close();
        chmod(MIGREATE_RECORD_FILE.c_str(), S_IRUSR | S_IWUSR);
    }
    auto retExecute =
        RmrsMigrateModule::DoMigrateExecute(migrateStrategyResult.vmInfoList, migrateStrategyResult.waitingTime);
    if (std::filesystem::exists(MIGREATE_RECORD_PATH)) {
        fout.open(MIGREATE_RECORD_FILE, std::ios::out | std::ios::binary);
        fout.close();
    }
    resp.len = 1;
    resp.data = new uint8_t[1];
    resp.freeFunc = [](uint8_t *data) -> void {
        delete[] data;
    };
    resp.data[0] = 0;
    if (retExecute != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemMigrate][MemMigrate] DoMigrateExecute failed, ret: " << retExecute << ".";
        resp.data[0] = 1;
    }
    return RMRS_OK;
}

RmrsResult CallbackManager::InitSmap()
{
    if (!isSetSmapMode) {
        RmrsResult retMode = RmrsSmapHelper::SmapMode(CallbackManager::memFragmentationMode);
        if (retMode == RMRS_OK) {
            UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsInit][Smap] Set memFragmentation mode success.";
        } else {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsInit][Smap] Set memFragmentation mode failed.";
            return RMRS_ERROR;
        }
        isSetSmapMode = true;
    } else {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit][Smap] MemFragmentation mode already set.";
    }

    return RMRS_OK;
}

RmrsResult CallbackManager::MigrateBackRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    if (req.data == nullptr || req.len == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemFree][MemFree] Request data is null, len= << req.len <<.";
        return RMRS_ERROR;
    }

    RmrsResult retSmap = CallbackManager::InitSmap();
    if (retSmap != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemFree][MemFree] InitSmap failed, retSmap: " << retSmap << ".";
        return RMRS_ERROR;
    }

    std::vector<uint16_t> numaIds;
    uint64_t memSize;
    RmrsResult res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    if (numaIds.empty()) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[MemFree][MemFree] No numa can return.";
    }
    MigrateBackResult migrateBackResult = {res, numaIds};
    RmrsOutStream builder;
    builder << migrateBackResult;
    resp.len = builder.GetSize();
    resp.data = builder.GetBufferPointer();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (RMRS_OK != res) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[MemFree][MemFree] Recv memfree res: " << res << ".";
        return RMRS_ERROR;
    }

    if (resp.data == nullptr || resp.len == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemMigrate][MemMigrate] Response data is null, len=" << resp.len <<".";
        return RMRS_ERROR;
    }

    return RMRS_OK;
}

RmrsResult CallbackManager::MigrateStrategyRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    if (req.data == nullptr || req.len == 0) {
        LOG_ERROR << "[MemMigrate][Strategy] Received empty request buffer.";
        return RMRS_ERROR;
    }

    RmrsResult retSmap = CallbackManager::InitSmap();
    if (retSmap != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to init smap, with ret: " << retSmap << ".";
        return RMRS_ERROR;
    }

    MigrateStrategyParamRMRS param;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    uint32_t waitingTime;

    RmrsResult res = RmrsMigrateModule::SetBorrowPlaneParam(param.pidRemoteNumaMap, param.timeOutNumas);
    if (res != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to set borrow plane param with ret: " << res << ".";
        return RMRS_ERROR;
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Param vm.size() = " << param.vmInfoList.size() << ".";
    res = RmrsMigrateModule::MigrateStrategyRmrs(param.borrowSize, param.vmInfoList, vmMigrateOutParam, waitingTime);
    if (res != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to get migrate strategy with ret: " << res << ".";
    } else {
        LOG_INFO << "[MemMigrate][Strategy] Getting migrate strategy successfully.";
    }

    LOG_DEBUG << "[MemMigrate][Strategy] WaitingTime (from output param): " << waitingTime << ".";
    for (size_t i = 0; i < vmMigrateOutParam.size(); i++) {
        LOG_DEBUG << "[MemMigrate][Strategy] VM to migrate (from output param): index[" << i << "], pid[ "
                  << vmMigrateOutParam[i].pid << "], MemSize[" << vmMigrateOutParam[i].memSize << "(KB)], DestNumaId["
                  << vmMigrateOutParam[i].desNumaId << "].";
    }

    MigrateStrategyResult result = {vmMigrateOutParam, waitingTime};
    RmrsOutStream builder;
    builder << result;
    resp.len = builder.GetSize();
    resp.data = builder.GetBufferPointer();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };

    if (resp.data == nullptr || resp.len == 0) {
        LOG_ERROR << "[MemMigrate][Strategy] Response data is null.";
        return RMRS_ERROR;
    }

    return RMRS_OK;
}

RmrsResult CallbackManager::PidNumaInfoCollectRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    const size_t maxPidNum = 300;
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ContainerNumaInfoCollect] Collect handler start.";
    if (req.data == nullptr || req.len == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerNumaInfoCollect] Received empty request buffer.";
        return RMRS_ERROR;
    }
    PidNumaInfoCollectParam param;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;
    std::vector<PidInfo> pidInfos;

    if (param.pidList.empty() || param.pidList.size() > maxPidNum) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ContainerNumaInfoCollect] pidList is invalid.";
        return RMRS_ERROR;
    }

    for (std::size_t i = 0; i < param.pidList.size(); i++) {
        if (param.pidList[i] <= 0) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ContainerNumaInfoCollect] pidList[" << i << "]=" << param.pidList[i] << ", is invalid.";
            return RMRS_ERROR;
        }
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerNumaInfoCollect] InParam: index[" << i << "] pid[ " << param.pidList[i] << "].";
    }

    RmrsResult res = ResourceExport::CollectPidNumaInfo(param.pidList, pidInfos);
    if (RMRS_OK != res) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerNumaInfoCollect] Recv ContainerNumaInfoCollect failed. res=" << res << ".";
        return RMRS_ERROR;
    }

    PidNumaInfoCollectResult result{pidInfos};
    RmrsOutStream builder;
    builder << result;
    resp.len = builder.GetSize();
    resp.data = builder.GetBufferPointer();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };

    for (const auto &pidInfo : result.pidInfoList) {
        // 访问pidInfo的字段
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerNumaInfoCollect] Deserialize pidInfo=" << pidInfo.ToString() << ".";
    }

    return RMRS_OK;
}

RmrsResult CallbackManager::NumaMemInfoCollectRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[NodeMemInfoCollect] Impl start.";
    if (req.data == nullptr || req.len == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[NodeMemInfoCollect] Received empty request buffer.";
        return RMRS_ERROR;
    }

    NumaMemInfoCollectParam param;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;
    if (param.numaId < -1) {
        RMRSResponseInfoSimpo response;
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[NodeMemInfoCollect] Param numaId is invalided.";
        SetResponse(response, RMRS_ERROR, "Param numaId is invalided", resp);
        return RMRS_ERROR;
    }
    std::map<std::string, std::string> meminfo;
    auto res = ResourceExport::CollectMeminfobyNumaId(param.numaId, meminfo);
    if (RMRS_OK != res) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[NodeMemInfoCollect] Recv NumaMemInfoCollect failed. res=" << res << ".";
        return RMRS_ERROR;
    }
    JSON_STR resJson;
    RMRSResponseInfoSimpo response;
    if (!rmrs::JsonUtil::RackMemConvertMap2JsonStr(meminfo, resJson)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertMap2JsonStr error.";
        SetResponse(response, RMRS_ERROR, "RackMemConvertMap2JsonStr error.", resp);
        return RMRS_ERROR;
    }
    SetResponse(response, res, resJson, resp);
    return res;
}

static void FreeMemory(uint8_t *data)
{
    delete[] data;
}

bool ValidatePid(pid_t pid)
{
    if (pid <= 0) {
        return false;
    }
    std::string path = "/proc/" + std::to_string(pid);
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

bool ValidatePidsParam(vector<pid_t> &pidsParam)
{
    const size_t maxPidNum = 300;
    if (pidsParam.size() > maxPidNum) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Too many pids in pidsParam, vectorSize=" << pidsParam.size() << ".";
        return false;
    }
    std::vector<pid_t> newList;
    for (auto pid : pidsParam) {
        if (ValidatePid(pid)) {
            newList.push_back(pid);
        } else {
            UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Invalid pid in pidsParam, pid=" << pid << ".";
        }
    }
    if (newList.size() == 0) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] No valid pids in pidsParam.";
        return false;
    }
    pidsParam = std::move(newList);
    return true;
}

int ReadNumaRemoteFile(int32_t numaId)
{
    const std::string basePath = "/sys/devices/system/node/";
    std::string remotePath = basePath + "node" + std::to_string(numaId) + "/remote";
    std::vector<std::string> fileLines;
    RMRS_RES result = RmrsFileUtil::GetFileInfo(remotePath, fileLines);
    if (result != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to open remote file of numaId=" << numaId << ".";
        return -1;
    }
    if (fileLines.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Remote file is empty: " << remotePath << ".";
        return -1;
    }
    std::string remoteValueStr = fileLines[0];
    int remoteValue = -1;
    try {
        remoteValue = std::stoi(remoteValueStr);
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Invalid NUMA ID format, numaStr=" << remoteValueStr << ".";
        return -1;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] NUMA ID out of range, numaStr=" << remoteValueStr << ".";
        return -1;
    }
    return remoteValue;
}

bool ValidateNumaIdParam(const UCacheMigrationStrategyParam &param)
{
    const std::string basePath = "/sys/devices/system/node/";
    if (param.remoteNumaIds.empty()) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] remoteNumaIds is empty!";
        return false;
    }
    for (auto remoteNumaId : param.remoteNumaIds) {
        if (ReadNumaRemoteFile(remoteNumaId) != 1) {
            UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Numa is not a valid remote numa, numaId=" << remoteNumaId << ".";
            return false;
        }
    }

    if (param.localNumaId >= 0 && ReadNumaRemoteFile(param.localNumaId) != 0) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Numa is not a valid local numa, numaId=" << param.localNumaId << ".";
        return false;
    }
    return true;
}

bool ValidateMigrationStrategyParameter(UCacheMigrationStrategyParam &param)
{
    if (!ValidatePidsParam(param.pids)) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Invalid pids in UCacheMigrationStrategyParam.";
        return false;
    }

    if (!ValidateNumaIdParam(param)) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Invalid numaId in UCacheMigrationStrategyParam.";
        return false;
    }

    if (param.ucacheUsageRatio < 0 || param.ucacheUsageRatio > 1) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Invalid ucacheUsageRatio in UCacheMigrationStrategyParam, ucacheUsageRatio="
            << param.ucacheUsageRatio << ".";
        return false;
    }
    return true;
}

RmrsResult CallbackManager::UCacheMigrateStrategyRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    if (req.data == nullptr || req.len == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Received empty request buffer.";
        return RMRS_ERROR;
    }

    UCacheMigrationStrategyParam param;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;
    if (!ValidateMigrationStrategyParameter(param)) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Invalid UCacheMigrationStrategy=" << param.ToString() << ".";
        ResCode result{};
        result.resCode = RMRS_ERROR;
        RmrsOutStream builder;
        builder << result;
        resp.data = builder.GetBufferPointer();
        resp.len = builder.GetSize();
        resp.freeFunc = FreeMemory;
        return RMRS_OK;
    }

    ucache::migrate_excutor::MigrationStrategy strategy = {
        .desNids = param.remoteNumaIds,
        .srcNid = param.localNumaId,
        .ucacheUsageRatio = param.ucacheUsageRatio,
        .pids = param.pids,
    };
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Recv UCacheMigrationStrategy: " << param.ToString() << ".";

    ResCode result{};
    result.resCode =
        ucache::migrate_excutor::UcacheMigrationExecutor::GetInstance().ExecuteNewMigrationStrategy(strategy);
    if (result.resCode != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Recv ExecuteNewMigrationStrategy error, res=" << result.resCode << ".";
    }
    RmrsOutStream builder;
    builder << result;
    resp.data = builder.GetBufferPointer();
    resp.len = builder.GetSize();
    resp.freeFunc = FreeMemory;
    return RMRS_OK;
}

RmrsResult CallbackManager::UCacheMigrateStopRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] recv UCacheMigrateStop.";
    ResCode result{};
    result.resCode = ucache::migrate_excutor::UcacheMigrationExecutor::GetInstance().StopCurrentMigrationStrategy();
    RmrsOutStream builder;
    builder << result;
    resp.data = builder.GetBufferPointer();
    resp.len = builder.GetSize();
    resp.freeFunc = FreeMemory;
    return RMRS_OK;
}

RmrsResult CallbackManager::UpdateUCacheRatioRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    if (req.data == nullptr || req.len == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Received empty request buffer.";
        return RMRS_ERROR;
    }
    const float percentConversionFactor = 100.0;
    MigrationInfoParam param{};
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Recv UpdateUCacheRatio=" << param.ToString() << ".";
    if (!ValidatePidsParam(param.pids)) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Invalid MigrationInfoParam=" << param.ToString() << ".";
        UCacheRatioRes result{};
        result.resCode = RMRS_ERROR;
        result.ucacheUsageRatio = 0.0;
        RmrsOutStream builder;
        builder << result;
        resp.data = builder.GetBufferPointer();
        resp.len = builder.GetSize();
        resp.freeFunc = FreeMemory;
        return RMRS_OK;
    }
    UCacheRatioRes result{};
    uint32_t ucacheUsagePercentage = 0;
    result.resCode = ucache::bottleneck_detector::BottleneckDetector::GetInstance().GetUCacheUsagePercentage(
        param, ucacheUsagePercentage);
    if (result.resCode != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] GetUCacheUsagePercentage failed.";
        result.ucacheUsageRatio = 0.0;
    } else {
        result.ucacheUsageRatio = static_cast<float>(ucacheUsagePercentage) / percentConversionFactor;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Local ucacheUsageRatio=" << result.ucacheUsageRatio << ".";
    RmrsOutStream builder;
    builder << result;
    resp.data = builder.GetBufferPointer();
    resp.len = builder.GetSize();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    return RMRS_OK;
}

} // namespace rmrs
