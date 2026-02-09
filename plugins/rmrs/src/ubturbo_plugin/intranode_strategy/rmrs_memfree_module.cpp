/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "rmrs_memfree_module.h"

#include "turbo_logger.h"
#include "turbo_conf.h"
#include "rmrs_smap_helper.h"
#include "rmrs_config.h"
#include "rmrs_resource_export.h"

namespace rmrs::migrate {
using namespace turbo::log;
using namespace rmrs::smap;

#define LOG_DEBUG UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_ERROR UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_INFO UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)

RmrsResult RmrsMemFreeModule::ReturnRemoteNuma(std::vector<ReturnVmInfo> &canReturnVmInfoList,
    std::vector<uint16_t> &numaIds)
{
    std::vector<uint16_t> remoteNumaIdList;
    std::vector<pid_t> pidsList;
    std::vector<uint64_t> memSizeList;
    for (ReturnVmInfo info : canReturnVmInfoList) {
        remoteNumaIdList.push_back(info.remoteNumaId);
        pidsList.push_back(info.pid);
        memSizeList.push_back(0);
    }
    // 可以归还的所有远端numa的合集
    std::unordered_set<uint16_t> unionSet(remoteNumaIdList.begin(), remoteNumaIdList.end());
    unionSet.insert(numaIds.begin(), numaIds.end());
    std::vector<uint16_t> unionList(unionSet.begin(), unionSet.end());
    // 查询smap管理的pid列表 与采集的可归还pid做合集去重 全部迁回
    for (uint16_t remoteNumaId : unionList) {
        // pidsList没有 但是查出来的list有
        std::vector<pid_t> exceptPidList;
        std::vector<ProcessPayload> processPayloadList;
        if (!RmrsSmapHelper::SmapQueryProcessConfigHelper(remoteNumaId, processPayloadList) &&
            GetExceptPidList(processPayloadList, pidsList, exceptPidList) && !exceptPidList.empty()) {
            for (pid_t pid : exceptPidList) {
                pidsList.push_back(pid);
                remoteNumaIdList.push_back(remoteNumaId);
                memSizeList.push_back(0);
            }
        }
    }
    RmrsResult res =
        RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIdList, pidsList, memSizeList, MIGRATEOUT_TIMEOUT);
    if (res != RMRS_OK) {
        LOG_ERROR << "[MemFree][MemFree] Smap Migrate failed.";
        return res;
    }
    // 移除pid进程管理
    res = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(remoteNumaIdList, pidsList);
    if (res != RMRS_OK) {
        LOG_ERROR << "[MemFree][MemFree] Rm pid mgr failed " << res << ".";
        return res;
    }
    std::set<uint16_t> uniqueRemoteNumaIdSet(remoteNumaIdList.begin(), remoteNumaIdList.end());
    for (uint16_t remoteNumaId : uniqueRemoteNumaIdSet) {
        numaIds.push_back(remoteNumaId);
    }
    return RMRS_OK;
}

bool RmrsMemFreeModule::GetExceptPidList(std::vector<ProcessPayload> processPayloadList, std::vector<pid_t> pidList,
                                         std::vector<pid_t> &exceptPidList)
{
    std::unordered_set<pid_t> pidSet(pidList.begin(), pidList.end());
    for (ProcessPayload payload : processPayloadList) {
        LOG_DEBUG << "[MemFree][MemFree] Get except pid list " << payload.l1Node[0] << " " << payload.l2Node[0] << " "
                  << payload.pid << ".";
        if (pidSet.find(payload.pid) == pidSet.end()) {
            exceptPidList.push_back(payload.pid);
        }
    }
    return true;
}

bool RmrsMemFreeModule::ReturnNumaInfoComparator(ReturnNumaInfo &info1, ReturnNumaInfo &info2)
{
    return info1.memUse < info2.memUse;
}

std::vector<ReturnVmInfo> RmrsMemFreeModule::GetCanReturnVmInfo(std::map<uint16_t, int> localNumaMemFreeMap,
                                                                std::vector<uint16_t> remoteNumaIdList,
                                                                std::vector<VmDomainInfo> &vmDomainInfos)
{
    std::vector<ReturnVmInfo> canReturnVmInfoSList;
    // 筛选当前节点所有虚机有运行在远端numa的虚机，并且远端占用的内存小于本地numa空余内存
    for (uint16_t tmpRemoteNumaId : remoteNumaIdList) {
        std::map<uint16_t, int> tmpLocalNumaMemFreeMap = localNumaMemFreeMap;
        std::set<pid_t> curNumaReturnPidSet;
        bool canReturn = true;
        for (VmDomainInfo vmDomainInfo : vmDomainInfos) {
            uint16_t curVmRemoteNumaId = vmDomainInfo.remoteNumaId;
            if (curVmRemoteNumaId == tmpRemoteNumaId && vmDomainInfo.metaData.pageSize != 0) {
                uint16_t localNumaId = vmDomainInfo.localNumaId;
                tmpLocalNumaMemFreeMap[localNumaId] -=
                    static_cast<int>(vmDomainInfo.remoteUsedMem / vmDomainInfo.metaData.pageSize);
                curNumaReturnPidSet.insert(vmDomainInfo.metaData.pid);
            }
        }
        // 若存在一个本地numa置换完所有远端虚机占用内存后小于0 则该远端numa不满足归还条件 下一个
        for (const auto &localNumaMemFreePair : tmpLocalNumaMemFreeMap) {
            if (localNumaMemFreePair.second < 0) {
                canReturn = false;
                break;
            }
        }
        if (canReturn) {
            for (pid_t pid : curNumaReturnPidSet) {
                ReturnVmInfo info;
                info.pid = pid;
                info.remoteNumaId = tmpRemoteNumaId;
                canReturnVmInfoSList.push_back(info);
            }
            // 更新本地numa内存信息
            localNumaMemFreeMap = tmpLocalNumaMemFreeMap;
        }
    }
    return canReturnVmInfoSList;
}

RmrsResult RmrsMemFreeModule::MemFreeImpl(std::vector<uint16_t> &numaIds)
{
    std::vector<NumaInfo> numaInfos;
    uint32_t queryNumaRes = rmrs::exports::ResourceExport::GetNumaInfoImmediately(numaInfos);
    if (queryNumaRes != RMRS_OK) {
        LOG_ERROR << "[MemFree] [MemFree] Query numa error.";
        return RMRS_ERROR;
    }
    if (numaInfos.empty()) {
        LOG_INFO << "[MemFree] [MemFree] CurNode numa empty.";
        return RMRS_OK;
    }
    ResolveReturnNumaIds(numaInfos, numaIds);
    std::vector<uint16_t> remoteNumaIdList;
    std::vector<ReturnNumaInfo> returnNumaInfoSumList;
    std::map<uint16_t, int> localNumaMemFreeMap;
    DistributeNumaInfo(numaInfos, returnNumaInfoSumList, localNumaMemFreeMap);
    SortRemoteNumaByMemUse(returnNumaInfoSumList, remoteNumaIdList);
    if (remoteNumaIdList.empty()) {
        LOG_INFO << "[MemFree] [MemFree] CurNode no remote numa.";
        return RMRS_OK;
    }
    // 查询虚机采集信息
    std::vector<VmDomainInfo> vmDomainInfos;
    uint32_t queryVmRes = rmrs::exports::ResourceExport::GetVmInfoImmediately(vmDomainInfos);
    if (queryVmRes != RMRS_OK) {
        LOG_ERROR << "[MemFree] [MemFree] Query vm error.";
        return RMRS_ERROR;
    }
    if (vmDomainInfos.empty()) {
        LOG_INFO << "[MemFree] [MemFree] CurNode vm empty.";
        return RMRS_OK;
    }
    std::vector<ReturnVmInfo> canReturnVmInfoList;
    canReturnVmInfoList = GetCanReturnVmInfo(localNumaMemFreeMap, remoteNumaIdList, vmDomainInfos);
    if (canReturnVmInfoList.empty()) {
        LOG_DEBUG << "[MemFree] [MemFree] All vm cant be migrate back.";
    } else {
        RmrsResult res = ReturnRemoteNuma(canReturnVmInfoList, numaIds);
        if (res != RMRS_OK) {
            return res;
        }
    }
    return RMRS_OK;
}

void RmrsMemFreeModule::ResolveReturnNumaIds(std::vector<NumaInfo> numaInfos, std::vector<uint16_t> &numaIds)
{
    for (NumaInfo info : numaInfos) {
        if (!info.numaMetaInfo.isLocal && info.numaMetaInfo.isRemoteAvailable &&
            info.numaMetaInfo.hugePageFree == info.numaMetaInfo.hugePageTotal && info.numaMetaInfo.memTotal != 0) {
            numaIds.push_back(info.numaMetaInfo.numaId);
        }
    }
}

void RmrsMemFreeModule::DistributeNumaInfo(std::vector<NumaInfo> &numaInfos,
    std::vector<ReturnNumaInfo> &returnNumaInfoSumList, std::map<uint16_t, int> &localNumaMemFreeMap)
{
    for (NumaInfo numaInfo : numaInfos) {
        ReturnNumaInfo info;
        uint16_t numaId = numaInfo.numaMetaInfo.numaId;
        info.numaId = numaId;
        info.memFree = numaInfo.numaMetaInfo.memFree;
        info.memUse = numaInfo.numaMetaInfo.memTotal - numaInfo.numaMetaInfo.memFree;
        info.hugePageFree = numaInfo.numaMetaInfo.hugePageFree;
        info.hugePageUse = numaInfo.numaMetaInfo.hugePageTotal - info.hugePageFree;
        if (numaInfo.numaMetaInfo.isLocal) {
            info.isLocal = true;
            localNumaMemFreeMap[numaId] = static_cast<int>(info.hugePageFree);
        } else {
            info.isLocal = false;
        }
        returnNumaInfoSumList.push_back(info);
    }
    return ;
}

void RmrsMemFreeModule::SortRemoteNumaByMemUse(std::vector<ReturnNumaInfo> &returnNumaInfoSumList,
                                               std::vector<uint16_t> &remoteNumaIdList)
{
    // 按照远端占用内存大小对远端numa进行排序放入remoteNumaIdList
    std::sort(returnNumaInfoSumList.begin(), returnNumaInfoSumList.end(), ReturnNumaInfoComparator);
    for (ReturnNumaInfo info : returnNumaInfoSumList) {
        if (!info.isLocal) {
            remoteNumaIdList.push_back(info.numaId);
        }
    }
}

#undef LOG_DEBUG
#undef LOG_ERROR
#undef LOG_INFO

}