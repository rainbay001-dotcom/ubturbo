/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "rmrs_rollback_module.h"

#include "turbo_logger.h"
#include "turbo_conf.h"
#include "rmrs_serialize.h"
#include "rmrs_json_helper.h"
#include "rmrs_config.h"
#include "rmrs_smap_helper.h"
#include "rmrs_resource_export.h"

namespace rmrs::migrate {

using namespace turbo::log;
using namespace rmrs::serialization;
using namespace rmrs::serialize;
using namespace rmrs::smap;

#define LOG_DEBUG UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_ERROR UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_INFO UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)

uint32_t RmrsRollbackModule::HandlerRollback(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    if (inputBuffer.len == 0 || inputBuffer.data == nullptr) {
        LOG_ERROR << "[MemRollback] Invalid input.";
        return RMRS_ERROR;
    }
    std::map<std::string, std::set<BorrowIdInfo>> value;
    RmrsInStream builderIn(inputBuffer.data, inputBuffer.len);
    builderIn >> value;
    for (const auto &[key, borrowSet] : value) {
        LOG_DEBUG << "[MemRollback] Prase input borrowIdInfo, borrowId = " << key << ".";
        for (const auto &borrow : borrowSet) {
            LOG_DEBUG << "[MemRollback] Prase input borrowIdInfo, pid= " << borrow.pid
                      << ", oriSize= " << borrow.oriSize << ".";
        }
    }
    auto ret = MemBorrowRollback(value);
    outputBuffer.len = 1;
    outputBuffer.data = new uint8_t[outputBuffer.len];
    outputBuffer.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (ret != 0) {
        LOG_ERROR << "[MemRollback] MemBorrowRollback failed.";
        outputBuffer.data[0] = 1;
        return RMRS_ERROR;
    }
    outputBuffer.data[0] = 0;
    return RMRS_OK;
}

uint32_t RmrsRollbackModule::MemBorrowRollback(
    std::map<std::string, std::set<rmrs::serialization::BorrowIdInfo>> &borrowIdsPidsMap)
{
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    std::vector<pid_t> pidList;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    std::vector<uint16_t> remoteNumaIdList;

    for (const auto &pair : borrowIdsPidsMap) {
        for (auto item : pair.second) {
            pidInfoList.insert(item);
            pidList.push_back(item.pid);
        }
    }
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    if (pidList.empty()) {
        return RMRS_ERROR;
    }
    // 全部迁回0
    if (!FillRollbackVmInfo(vmInfoMap, pidList, vmPidRemoteNumaMap, remoteNumaIdList) ||
        !CanMigrate(vmInfoMap, pidInfoList) || !DoMigrateRollback(vmPidRemoteNumaMap, pidInfoList)) {
        return RMRS_ERROR;
    }
    // 开启对应numa的冷热流动
    for (uint16_t remoteNumaId : remoteNumaIdList) {
        if (RmrsSmapHelper::SmapEnableRemoteNuma(remoteNumaId)) {
            LOG_ERROR << "[MemRollback] SmapEnableRemoteNuma error.";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

bool RmrsRollbackModule::FillRollbackVmInfo(std::map<pid_t, VmDomainInfo> &vmInfoMap, std::vector<pid_t> &pidList,
    std::unordered_map<pid_t, uint16_t> &vmPidRemoteNumaMap, std::vector<uint16_t> &remoteNumaIdList)
{
    // 查询虚机采集信息
    std::vector<VmDomainInfo> vmDomainInfos;
    uint32_t queryVmRes = rmrs::exports::ResourceExport::GetVmInfoImmediately(vmDomainInfos);
    if (queryVmRes != 0) {
        LOG_ERROR << "[MemRollback] CurNode query vm error.";
        return false;
    }
    if (vmDomainInfos.empty()) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[MemRollback] CurNode vm empty.";
        return true;
    }
    LOG_DEBUG << "[MemRollback] PidList size = " << pidList.size() << ".";
    for (auto &pid : pidList) {
        LOG_DEBUG << "[MemRollback] Pid = " << pid << ".";
    }

    for (VmDomainInfo vmDomainInfo : vmDomainInfos) {
        LOG_DEBUG << "[MemRollback] VmDomainInfo.metaData.pid = " << vmDomainInfo.metaData.pid << ".";
        if (std::find(pidList.begin(), pidList.end(), vmDomainInfo.metaData.pid) != pidList.end()) {
            LOG_DEBUG << "[MemRollback] VmDomainInfo.remoteNumaId = " << vmDomainInfo.remoteNumaId << ".";
            if (vmDomainInfo.remoteNumaId != 0) {
                pid_t pid = vmDomainInfo.metaData.pid;
                vmInfoMap[pid] = vmDomainInfo;
                vmPidRemoteNumaMap[pid] = vmDomainInfo.remoteNumaId;
                remoteNumaIdList.push_back(vmDomainInfo.remoteNumaId);
            }
        }
    }
    return true;
}

bool RmrsRollbackModule::CanMigrate(std::map<pid_t, VmDomainInfo> &vmInfoMap,
    const std::set<rmrs::serialization::BorrowIdInfo> &pidInfoList)
{
    if (vmInfoMap.empty()) {
        LOG_INFO << "[MemRollback] VmInfoMap empty.";
        return true;
    }
    std::vector<NumaInfo> numaInfos;
    uint32_t queryNumaRes = rmrs::exports::ResourceExport::GetNumaInfoImmediately(numaInfos);
    if (queryNumaRes != 0) {
        LOG_ERROR << "[MemRollback] CurNode query numa error.";
        return false;
    }
    if (numaInfos.empty()) {
        LOG_ERROR << "[MemRollback] CurNode numa empty.";
        return false;
    }
    std::map<uint16_t, NumaInfo> numaInfoMap;
    for (NumaInfo NumaInfo : numaInfos) {
        numaInfoMap[NumaInfo.numaMetaInfo.numaId] = NumaInfo;
    }
    return CanMigrateBack(vmInfoMap, numaInfoMap, pidInfoList);
}

bool RmrsRollbackModule::CanMigrateBack(std::map<pid_t, VmDomainInfo> &vmInfoMap,
    std::map<uint16_t, NumaInfo> &numaInfoMap,
    const std::set<rmrs::serialization::BorrowIdInfo> &pidInfoList)
{
    // 最终需要占用本地numa内存大小
    std::unordered_map<uint16_t, uint64_t> fowardSumMemSizeMap;
    uint64_t pageSize = RmrsMigrateModule::memoryPageSize;
    for (auto vmInfoEntry : vmInfoMap) {
        pid_t pid = vmInfoEntry.first;
        uint64_t oriSize{0};
        for (const auto &info : pidInfoList) {
            if (info.pid == pid) {
                oriSize = info.oriSize;
            }
        }
        VmDomainInfo vmDomainInfo = vmInfoEntry.second;
        // 这里fowardSumMemSize表示这个虚拟机要迁回的，应该减去这个虚拟机原来占用的
        if (vmDomainInfo.remoteUsedMem < oriSize) {
            LOG_ERROR << "[MemRollback] remoteUsedMem(" << vmDomainInfo.remoteUsedMem << ") < oriSize(" << oriSize
                      << ") for pid: " << pid;
            return false;
        }
        uint64_t fowardSumMemSize = vmDomainInfo.remoteUsedMem - oriSize;
        // 远端占用内存不够一页的算一页
        uint64_t pageNum = fowardSumMemSize / pageSize;
        if (fowardSumMemSize % pageSize != 0) {
            pageNum++;
        }
        fowardSumMemSizeMap[vmDomainInfo.localNumaId] += pageNum;
    }

    for (auto entry : fowardSumMemSizeMap) {
        if (entry.second > numaInfoMap[entry.first].numaMetaInfo.hugePageFree) {
            LOG_ERROR << "[MemRollback] Local page no enough failed " << entry.second << ".";
            return false;
        }
    }
    return true;
}

bool RmrsRollbackModule::DoMigrateRollback(std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap,
    const std::set<rmrs::serialization::BorrowIdInfo> &pidInfoList)
{
    if (vmPidRemoteNumaMap.empty()) {
        LOG_INFO << "[MemRollback] No vm need rollback.";
        return true;
    }
    std::vector<uint16_t> remoteNumaIdList;
    std::vector<pid_t> pidList;
    std::vector<uint64_t> memSizeList;
    for (auto vmPidRemoteNumaEntry : vmPidRemoteNumaMap) {
        remoteNumaIdList.push_back(vmPidRemoteNumaEntry.second);
        pidList.push_back(vmPidRemoteNumaEntry.first);
        uint64_t oriSize{0};
        for (const auto &info : pidInfoList) {
            if (info.pid == vmPidRemoteNumaEntry.first) {
                oriSize = info.oriSize;
                break;
            }
        }
        memSizeList.push_back(oriSize);
    }
    RmrsResult res =
        RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIdList, pidList, memSizeList, MIGRATEOUT_TIMEOUT);
    if (res != 0) {
        LOG_ERROR << "[MemRollback] DoMigrateRollback failed " << res << ".";
        return false;
    }
    // 移除pid进程管理
    // oriSize为0的才考虑移除进程管理
    std::vector<pid_t> emptyPidList; // 存储 memSize == 0 的 pid
    std::vector<uint16_t> emptyRemoteNumaIdList;

    for (size_t i = 0; i < pidList.size(); ++i) {
        if (memSizeList[i] == 0) {
            emptyPidList.push_back(pidList[i]);
            emptyRemoteNumaIdList.push_back(remoteNumaIdList[i]);
        }
    }
    // 移除pid进程管理
    if (emptyPidList.empty()) {
        return true;
    }
    res = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(emptyRemoteNumaIdList, emptyPidList);
    if (res != 0) {
        LOG_ERROR << "[MemRollback] Rm pid mgr failed " << res << ".";
        return false;
    }
    return true;
}

#undef LOG_DEBUG
#undef LOG_ERROR
#undef LOG_INFO

}