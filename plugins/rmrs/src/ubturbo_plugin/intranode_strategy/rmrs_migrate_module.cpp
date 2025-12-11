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
#include "rmrs_migrate_module.h"

#include <algorithm>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <vector>
#include <cstdint>
#include <limits>

#include "rmrs_resource_export.h"

#include "rmrs_config.h"
#include "rmrs_error.h"
#include "rmrs_json_helper.h"
#include "rmrs_libvirt_module.h"
#include "rmrs_smap_helper.h"
#include "turbo_logger.h"
#include "turbo_conf.h"
#include "securec.h"

namespace rmrs::migrate {

using namespace turbo::log;
using namespace turbo::config;
using namespace rmrs::smap;
using namespace rmrs::libvirt;
using namespace rmrs::serialization;
using namespace rmrs::exports;
using namespace rmrs::smap;

const uint64_t RmrsMigrateModule::memoryPageSize = 2048;
const uint64_t RmrsMigrateModule::memoryThreshold = 2048; // 2048kb; 最小迁移2M
const uint64_t RmrsMigrateModule::memoryPreset = 100;
const uint64_t RmrsMigrateModule::smapMigrateOutWaitingTimeMin = 10001; // 内存迁出最小等待时间 10S + 1ms
const uint64_t RmrsMigrateModule::smapMigrateOutWaitingTimeMax = 59999; // 内存迁出最大等待时间 60s - 1ms
const double RmrsMigrateModule::ratioDiff = 0.01;                       // 迁移比例允许取整差值 0.01
const uint16_t RmrsMigrateModule::freqNumZero = 0;                      // 频次等于0的
const uint16_t RmrsMigrateModule::freqNumOne = 1;                       // 频次等于1的
const uint16_t RmrsMigrateModule::freqNumTwo = 2;                       // 频次等于2的
const uint16_t RmrsMigrateModule::freqNumThree = 3;                     // 频次等于3的
const uint16_t RmrsMigrateModule::freqNumFour = 4;                      // 频次等于4的
const uint64_t RmrsMigrateModule::freqScoreSeven = 7;                   // 分数等于7的
const uint64_t RmrsMigrateModule::freqScoreTwo = 2;                     // 分数等于2的
const uint64_t RmrsMigrateModule::freqScoreOne = 1;                     // 分数等于1的
const uint32_t RmrsMigrateModule::smapMigrateCycleTime = 2000;          // smap每个迁移周期时间 2秒 = 2000ms
const uint32_t RmrsMigrateModule::smapMigrateBaseCycle = 1;             // 1个迁移周期起步
const uint32_t RmrsMigrateModule::smapMigrate32GCycle = 3;              // 每增加32G增加3个迁移周期
const uint32_t RmrsMigrateModule::smapMigrateUnitMem = 32;              // 32G
const uint64_t RmrsMigrateModule::byte2KB = 1024;                       // bite转为KB 1024单位转换

std::vector<uint16_t> RmrsMigrateModule::s_timeOutNumas; // 超时归还的numa信息
std::map<pid_t, std::vector<uint16_t>> RmrsMigrateModule::s_pidRemoteNumaMap; // pid对应的远端numa信息Map

#define LOG_DEBUG UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_ERROR UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_INFO UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
#define LOG_WARN UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)

// 获取 numaInfoMap 所有numa节点的索引对应的NUMA信息(numaID 大页总数量 大页空闲数量 是否本地 是否远端)
void RmrsMigrateModule::DistributeNumaMemInfo(std::vector<rmrs::NumaInfo> &numaInfos,
                                              std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                                              std::vector<NumaHugePageInfo> &numaHugePageInfoSumList)
{
    for (rmrs::NumaInfo NumaInfoWithSize : numaInfos) {
        NumaHugePageInfo info;
        info.numaId = NumaInfoWithSize.numaMetaInfo.numaId;
        info.hugePageFree = NumaInfoWithSize.numaMetaInfo.hugePageFree;
        info.hugePageTotal = NumaInfoWithSize.numaMetaInfo.hugePageTotal;
        info.isRemote = NumaInfoWithSize.numaMetaInfo.isRemoteAvailable; // 可以迁出内存的numa节点 才是远端numa节点
        info.isLocal = NumaInfoWithSize.numaMetaInfo.isLocal;
        info.isTimeOut = false;

        // 在超时numa里面找到该numa节点就标记一下超时
        auto it = std::find(s_timeOutNumas.begin(), s_timeOutNumas.end(), info.numaId);
        if (it != s_timeOutNumas.end()) {
            LOG_WARN << "[MemMigrate][Strategy] Current numa = " << info.numaId << " is timeOut.";
            info.isTimeOut = true;
        }

        numaInfoMap[info.numaId] = info;
        numaHugePageInfoSumList.push_back(info);

        LOG_DEBUG << "[MemMigrate][Strategy][DistributeNumaMemInfo] NUMA node info: numaId = " << info.numaId
                  << ", hugePageFree = " << info.hugePageFree << ", hugePageTotal = " << info.hugePageTotal
                  << ", isRemote = " << info.isRemote << ", isLocal = " << info.isLocal
                  << ", isTimeOut = " << info.isTimeOut
                  << ", socketId = " << NumaInfoWithSize.numaMetaInfo.socketId << ".";
    }
}

// 获取远端numaID数组remoteNumaIdList
void GetRemoteNumaList(std::vector<NumaHugePageInfo> &numaHugePageInfoSumList, std::vector<uint16_t> &remoteNumaIdList)
{
    for (NumaHugePageInfo info : numaHugePageInfoSumList) {
        if (info.isLocal == false && info.isRemote == true && info.isTimeOut == false) {
            remoteNumaIdList.push_back(info.numaId);
            LOG_DEBUG << "[MemMigrate][Strategy] The remoteNumaIdList push_back " << info.numaId << ".";
        }
    }
    LOG_DEBUG << "[MemMigrate][Strategy] The remoteNumaIdList size() = " << remoteNumaIdList.size() << ".";
}

// 打印查询虚拟机信息
void GetLocalVmInfo(std::vector<VmNumaInfo> &allVmNumaInfoInfoList, std::map<pid_t, VmNumaInfo> &VmNumaInfoMap,
                    std::vector<VmDomainInfo> &vmDomainInfos)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Enter GetLocalVmInfo.";

    if (vmDomainInfos.size() == 0) {
        LOG_ERROR << "[MemMigrate][Strategy] VM data is empty. Nothing to print.";
        return;
    }

    for (VmDomainInfo vmDomainInfo : vmDomainInfos) {
        VmNumaInfo info;
        info.pid = vmDomainInfo.metaData.pid;
        info.localNumaId = vmDomainInfo.localNumaId;
        info.remoteNumaId = vmDomainInfo.remoteNumaId;
        info.localUsedMem = vmDomainInfo.localUsedMem;
        info.remoteUsedMem = vmDomainInfo.remoteUsedMem;
        allVmNumaInfoInfoList.push_back(info);
        VmNumaInfoMap[info.pid] = info;
        LOG_DEBUG << "[MemMigrate][Strategy] VM info with PID = " << info.pid << ".";
        LOG_DEBUG << "[MemMigrate][Strategy] PID = " << info.pid << ", localNumaId = " << info.localNumaId << ".";
        LOG_DEBUG << "[MemMigrate][Strategy] PID = " << info.pid << ", remoteNumaId = " << info.remoteNumaId << ".";
        LOG_DEBUG << "[MemMigrate][Strategy] PID = " << info.pid << ", localUsedMem = " << info.localUsedMem << ".";
        LOG_DEBUG << "[MemMigrate][Strategy] PID = " << info.pid << ", remoteUsedMem = " << info.remoteUsedMem << ".";
        uint16_t socketId = vmDomainInfo.metaData.socketId;
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemMigrate][Strategy] PID = " << info.pid << ", socketId = " << socketId << ".";
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Exit GetLocalVmInfo.";
}

// 判断远端内存是否足够
bool RmrsMigrateModule::ISRemoteMemorySufficient(std::vector<uint16_t> &remoteNumaIdList,
                                                 std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                                                 const uint64_t &memMigrateTotalSize)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Enter ISRemoteMemorySufficient.";
    uint64_t pagesRemoteFreeSum = 0;
    for (auto numaId : remoteNumaIdList) {
        auto it = numaInfoMap.find(numaId);
        if (it != numaInfoMap.end()) {
            pagesRemoteFreeSum += it->second.hugePageFree;
            LOG_DEBUG << "[MemMigrate][Strategy] Current node, remoteNumaId = " << numaId
                      << ", hugePageFreeSum = " << it->second.hugePageFree << ".";
        }
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Current node, remoteNumaHugePageFreeNumSum = " << pagesRemoteFreeSum << ".";

    uint64_t hugePagesMigrate = memMigrateTotalSize / RmrsMigrateModule::memoryPageSize;
    LOG_DEBUG << "[MemMigrate][Strategy] Input param, hugePagesNeedToMigrate = " << hugePagesMigrate << ".";

    if (hugePagesMigrate > pagesRemoteFreeSum) {
        LOG_ERROR << "[MemMigrate][Strategy] The hugePagesMigrate(" << hugePagesMigrate << ") > pagesRemoteFreeSum("
                  << pagesRemoteFreeSum << ").";
        return false;
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Exit ISRemoteMemorySufficient.";
    return true;
}

bool RmrsMigrateModule::ISPresetMemorySufficient(const uint64_t &memMigrateTotalSize,
                                                 const std::map<pid_t, VmNumaInfo> &vmNumaInfoMap,
                                                 const std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
                                                 std::map<pid_t, uint64_t> &vmMigratableMemMap)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Enter ISPresetMemorySufficient.";
    uint64_t curPresetMem = 0;
    uint64_t tmpCanMigrateMem = 0;

    for (auto vmParam : vmPresetParam) {
        auto it = vmNumaInfoMap.find(vmParam.pid);
        if (it == vmNumaInfoMap.end()) {
            LOG_ERROR << "[MemMigrate][Strategy] Invalid PID: VM not found in vmNumaInfoMap. PID = " << vmParam.pid
                      << ".";
            return false;
        }

        uint64_t tmpLocalMem = it->second.localUsedMem;   // VM本地使用内存 单位kb
        uint64_t tmpRemoteMem = it->second.remoteUsedMem; // VM远端使用内存 单位kb
        uint64_t tmpMemSum = tmpLocalMem + tmpRemoteMem;  // VM内存总量 单位kb
        double presetMigrateMem = (static_cast<double>(tmpMemSum) * (static_cast<double>(vmParam.ratio))) /
                                  static_cast<double>(RmrsMigrateModule::memoryPreset); // VM预设可以迁出的内存大小
        double realMigrateMem = presetMigrateMem - static_cast<double>(tmpRemoteMem); // VM实际可以迁出的内存大小
        LOG_DEBUG << "[MemMigrate][Strategy] The VM, pid = " << vmParam.pid << ", ratio = " << vmParam.ratio
                  << "%, memSum = " << tmpMemSum << "(KB), realMigrateMem = " << realMigrateMem << "(KB).";
        if (realMigrateMem < 0) {
            LOG_ERROR << "[MemMigrate][Strategy] The preset migration ratio is smaller than the actual migrated ratio.";
            return false;
        }

        // 对实际还能迁出的内存(KB), 向下取整为2048的倍数
        tmpCanMigrateMem = static_cast<uint64_t>(realMigrateMem) / RmrsMigrateModule::memoryPageSize *
                           RmrsMigrateModule::memoryPageSize;
        vmMigratableMemMap[vmParam.pid] = tmpCanMigrateMem;
        curPresetMem += tmpCanMigrateMem;
        LOG_DEBUG << "[MemMigrate][Strategy] The VM, pid = " << vmParam.pid << ", ratio = " << vmParam.ratio
                  << "%, max migration Mem = " << tmpCanMigrateMem << "(KB).";
    }

    if (curPresetMem < memMigrateTotalSize) {
        LOG_ERROR << "[MemMigrate][Strategy] The amount of memory(" << curPresetMem
                  << "KB) that can actually be migrated is less than the amount(" << memMigrateTotalSize
                  << "KB) required for migration.";
        return false;
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Exit ISPresetMemorySufficient.";
    return true;
}

void RmrsMigrateModule::GetMigrateOutTime(const uint64_t &memMigrateTotalSize, uint32_t &waitingTime)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Entry GetMigrateOutTime.";
    // 定义迁移周期时间为2秒，即2000毫秒
    const uint32_t migrateCycleTime = RmrsMigrateModule::smapMigrateCycleTime; // 每个迁移周期时间 2秒 = 2000ms
    const uint32_t migrateBaseCycle = RmrsMigrateModule::smapMigrateBaseCycle; // 1个迁移周期起步
    const uint32_t migrate32GCycle = RmrsMigrateModule::smapMigrate32GCycle;   // 每增加32G增加3个迁移周期
    const uint32_t migrateBaseMem = RmrsMigrateModule::smapMigrateUnitMem;     // 32G

    // 将memMigrateTotalSize从KB转换为GB
    uint64_t memMigrateTotalSizeGB = (memMigrateTotalSize) / (RmrsMigrateModule::byte2KB * RmrsMigrateModule::byte2KB);

    // 1个迁移周期起步，每32GB增加3个迁移周期
    double cycles = static_cast<double>(migrateBaseCycle) +
                    (static_cast<double>(memMigrateTotalSizeGB) * static_cast<double>(migrate32GCycle)) /
                        static_cast<double>(migrateBaseMem);

    // 计算等待时间
    waitingTime = static_cast<uint32_t>(cycles * static_cast<double>(migrateCycleTime));
    if (waitingTime < smapMigrateOutWaitingTimeMin) {
        waitingTime = smapMigrateOutWaitingTimeMin;
    } else if (waitingTime > smapMigrateOutWaitingTimeMax) {
        waitingTime = smapMigrateOutWaitingTimeMax;
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Input param memMigrateTotalSize = " << memMigrateTotalSize << " KB.";
    LOG_DEBUG << "[MemMigrate][Strategy] Output param waitingTime = " << waitingTime << " ms.";
    LOG_DEBUG << "[MemMigrate][Strategy] Exit GetMigrateOutTime.";
}

// 获取NUMA数据
RmrsResult RmrsMigrateModule::GetNumaData(std::vector<rmrs::NumaInfo> &numaInfos,
                                          std::vector<uint16_t> &remoteNumaIdList,
                                          std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                                          std::vector<NumaHugePageInfo> &numaHugePageInfoSumList)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Enter GetNumaData.";
    // 获取当前节numa信息
    uint32_t ret = rmrs::exports::ResourceExport::GetNumaInfoImmediately(numaInfos);
    if (RMRS_OK != ret) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to get NUMA info.";
        return RMRS_ERROR;
    }
    if (numaInfos.empty()) {
        LOG_ERROR << "[MemMigrate][Strategy] Current node has no NUMA data.";
        return RMRS_ERROR;
    }

    // 获取 numaInfoMap 所有numa节点的索引对应的NUMA信息(numaID 大页总数量 大页空闲数量 是否本地 是否远端)
    DistributeNumaMemInfo(numaInfos, numaInfoMap, numaHugePageInfoSumList);
    // 获取远端numaID数组remoteNumaIdList
    GetRemoteNumaList(numaHugePageInfoSumList, remoteNumaIdList);
    // 从小到大排序
    std::sort(remoteNumaIdList.begin(), remoteNumaIdList.end());
    if (remoteNumaIdList.empty()) {
        LOG_ERROR << "[MemMigrate][Strategy] Current node has no remote NUMA node.";
        return RMRS_ERROR;
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Exit GetNumaData.";
    return RMRS_OK;
}

// 获取虚拟机信息
RmrsResult RmrsMigrateModule::GetVMData(std::vector<rmrs::VmDomainInfo> &vmDomainInfos,
                                        std::vector<VmNumaInfo> &allVmNumaInfoInfoList,
                                        std::map<pid_t, VmNumaInfo> &vmNumaInfoMap)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Enter GetVMData.";

    RmrsResult ret = rmrs::exports::ResourceExport::GetVmInfoImmediately(vmDomainInfos);
    if (RMRS_OK != ret) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to get VM info.";
        return RMRS_ERROR;
    }
    if (vmDomainInfos.empty()) {
        LOG_INFO << "[MemMigrate][Strategy] Current node has no VM.";
        return RMRS_ERROR;
    }

    // 提取 allVmNumaInfoInfoList, vmNumaInfoMap数据
    GetLocalVmInfo(allVmNumaInfoInfoList, vmNumaInfoMap, vmDomainInfos); // 获取虚拟机信息

    LOG_DEBUG << "[MemMigrate][Strategy] Exit GetVMData.";
    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::CheckNumaAndVMStatus(const uint64_t &memMigrateTotalSize,
                                                   std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
                                                   NumaQueryInfo &numaQueryInfo, VMQueryInfo &vmQueryInfo)
{
    // 获取当前节点numa信息
    RmrsResult ret = GetNumaData(numaQueryInfo.numaInfos, numaQueryInfo.remoteNumaIdList, numaQueryInfo.numaInfoMap,
                                 numaQueryInfo.numaHugePageInfoSumList);
    if (ret != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to get NUMA data.";
        return RMRS_ERROR;
    }

    // 获取虚拟机信息
    ret = GetVMData(vmQueryInfo.vmDomainInfos, vmQueryInfo.allVmNumaInfoInfoList, vmQueryInfo.vmNumaInfoMap);
    if (ret != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to get VM data.";
        return RMRS_ERROR;
    }
    // 如果输入的超时远端numa中有本地的或者不存在的，返回ERROR
    ret = CheckTimeOutNuma(numaQueryInfo.numaHugePageInfoSumList);
    if (ret != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Input remote numa ERROR.";
        return RMRS_ERROR;
    }

    // 如果输入的虚拟机列表里有在超时numa上面的就剔除掉
    ret = RemoveVmOfTimeOutNuma(vmQueryInfo.vmNumaInfoMap, vmPresetParam);
    if (ret != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to remove vm of timeOut numa.";
        return RMRS_ERROR;
    }

    // 判断远端内存数量是否足够
    bool isSufficient =
        ISRemoteMemorySufficient(numaQueryInfo.remoteNumaIdList, numaQueryInfo.numaInfoMap, memMigrateTotalSize);
    if (!isSufficient) {
        LOG_ERROR << "[MemMigrate][Strategy] Hugepages on the remote NUMA node are not enough.";
        return RMRS_ERROR;
    }

    // 判断虚拟机预设最大迁出内存是否足够
    bool isPreMemMigrateEnough = ISPresetMemorySufficient(memMigrateTotalSize, vmQueryInfo.vmNumaInfoMap, vmPresetParam,
                                                          vmQueryInfo.vmMigratableMemMap);
    if (!isPreMemMigrateEnough) {
        LOG_ERROR << "[MemMigrate][Strategy] The VM preset max migrate memory is not enough.";
        return RMRS_ERROR;
    }

    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::MigrateStrategyRmrs(
    const uint64_t &memMigrateTotalSize, std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
    std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam, uint32_t &waitingTime)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Call MigrateStrategyRmrs.";
    LOG_DEBUG << "[MemMigrate][Strategy] Input Param: memMigrateTotalSize = " << memMigrateTotalSize << " (KB).";
    if (vmPresetParam.size() == 0) {
        LOG_ERROR << "[MemMigrate][Strategy] The input vm parameter is invalid.";
        vmMigrateOutParam.clear();
        waitingTime = 0;
        return RMRS_ERROR;
    }

    for (size_t i = 0; i < vmPresetParam.size(); i++) {
        LOG_DEBUG << "[MemMigrate][Strategy] Input Param: pid = " << vmPresetParam[i].pid
                  << ", ratio = " << vmPresetParam[i].ratio << ".";
        if (vmPresetParam[i].ratio > RmrsMigrateModule::memoryPreset) {
            LOG_ERROR << "[MemMigrate][Strategy] The input ratio parameter is invalid.";
            vmMigrateOutParam.clear();
            waitingTime = 0;
            return RMRS_ERROR;
        }
    }

    NumaQueryInfo numaQueryInfo; // 获取当前节点numa信息
    VMQueryInfo vmQueryInfo;     // 获取虚拟机信息
    // 查询NUMA、VM信息，判断是否满足迁移条件
    RmrsResult ret = CheckNumaAndVMStatus(memMigrateTotalSize, vmPresetParam, numaQueryInfo, vmQueryInfo);
    if (ret != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] NUMA or VM information does not meet the required conditions.";
        vmMigrateOutParam.clear();
        waitingTime = 0;
        return RMRS_ERROR;
    }

    // 深度优先搜索剪枝算法获取迁出动作集合
    ret = DFSGetMigrationActions(memMigrateTotalSize, numaQueryInfo, vmQueryInfo, vmMigrateOutParam);
    if (ret != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to obtain migration action set using pruning algorithm.";
        vmMigrateOutParam.clear();
        waitingTime = 0;
        return RMRS_ERROR;
    }

    // 预期迁出时间
    GetMigrateOutTime(memMigrateTotalSize, waitingTime);

    LOG_INFO << "[MemMigrate][Strategy] Migration strategy successful.";

    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::FillVmOriginSize(
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParamList,
    std::unordered_map<pid_t, uint64_t> &vmOriginSizeMap)
{
    std::vector<VmDomainInfo> vmDomainInfoList;
    uint32_t queryVmRes = rmrs::exports::ResourceExport::GetVmInfoImmediately(vmDomainInfoList);
    if (queryVmRes != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_NAME) << "[MemMigrate] [MemMigrate] Query vm error.";
        return RMRS_ERROR;
    }
    if (vmDomainInfoList.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_NAME) << "[MemMigrate] [MemMigrate] CurNode vm empty.";
        return RMRS_ERROR;
    }
    for (rmrs::serialization::VMMigrateOutParam migrateParam : vmMigrateOutParamList) {
        bool notFind = true;
        for (VmDomainInfo info : vmDomainInfoList) {
            if (migrateParam.pid == info.metaData.pid) {
                notFind = false;
                vmOriginSizeMap[migrateParam.pid] = info.remoteUsedMem;
            }
        }
        if (notFind) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_NAME) << "[MemMigrate] [MemMigrate] Vm not exist, pid="
                << migrateParam.pid << ".";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::DoMigrateExecute(
    const std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam, uint64_t waitingTime)
{
    // 获取迁移前虚机在远端大小
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap;
    if (waitingTime != 0 && FillVmOriginSize(vmMigrateOutParam, vmOriginSizeMap)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemMigrate][MemMigrate] Fill vm origin size failed.";
        return RMRS_ERROR;
    }
    // 这里把相同pid的拼起来
    std::vector<rmrs::serialization::VMMigrateMultiOutParam> vmMigrateMultiOutParam;
    std::unordered_map<pid_t, VMMigrateMultiOutParam> mergeMap;

    for (const auto &item : vmMigrateOutParam) {
        auto &multi = mergeMap[item.pid];
        multi.pid = item.pid;
        multi.memSize.push_back(item.memSize);
        multi.desNumaId.push_back(item.desNumaId);
    }
    vmMigrateMultiOutParam.reserve(mergeMap.size());
    for (auto &[pid, param] : mergeMap) {
        vmMigrateMultiOutParam.push_back(std::move(param));
    }
    std::vector<rmrs::serialization::VMMigrateMultiOutParam> executedParamList;
    for (size_t i = 0; i < vmMigrateMultiOutParam.size(); i++) {
        rmrs::serialization::VMMigrateMultiOutParam param = vmMigrateMultiOutParam[i];
        executedParamList.push_back(param);
        std::vector<uint16_t> remoteNumaIds = param.desNumaId;
        std::vector<pid_t> pids(remoteNumaIds.size(), param.pid);
        std::vector<uint64_t> memSizeList = param.memSize;
        RmrsResult res = RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIds, pids, memSizeList, waitingTime);
        if (res != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[MemMigrate][MemMigrate] MigrateOutExecute failed pid=" << param.pid << ", res=" << res << ".";
            if (waitingTime != 0) {
                RmrsMigrateModule::RollbackVmMigrate(executedParamList, vmOriginSizeMap);
            }
            return RMRS_ERROR;
        }
        if (waitingTime != 0 && param.memSize[0] == 0) {
            res = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(pids);
            if (res != RMRS_OK) {
                LOG_ERROR << "[MemMigrate][MemMigrate] Rm pid mgr failed" << res << ".";
            }
        }
    }

    return RMRS_OK;
}

void RmrsMigrateModule::RollbackVmMigrate(
    const std::vector<rmrs::serialization::VMMigrateMultiOutParam> &executedParamList,
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap)
{
    for (size_t i = 0; i < executedParamList.size(); i++) {
        rmrs::serialization::VMMigrateMultiOutParam param = executedParamList[i];
        std::vector<uint16_t> remoteNumaIds = param.desNumaId;
        std::vector<pid_t> pids = {param.pid};
        std::vector<uint64_t> memSizeList = {vmOriginSizeMap[param.pid]};
        RmrsResult res =
            RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIds, pids, memSizeList, MIGRATEOUT_TIMEOUT);
        if (res == RMRS_OK && memSizeList[0] == 0) {
            // 移除pid进程管理
            res = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(pids);
            if (res != RMRS_OK) {
                LOG_ERROR << "rm pid mgr failed" << res;
            }
        } else if (res != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "MigrateOutExecute rollback failed pid: " << param.pid << " res: " << res;
        }
    }
}

bool RmrsMigrateModule::CanPrune(const DFSContext &ctx, size_t index, uint64_t currentTotal)
{
    uint64_t remaining = ctx.totalRequired - currentTotal;
    uint64_t futureMax = 0;
    for (size_t i = index; i < ctx.vms.size(); ++i) {
        futureMax += ctx.vms[i].maxSize;
    }
    if (futureMax < remaining) {
        return true;
    }

    uint64_t totalFree = 0;
    for (const auto &pair : ctx.numaFreeMap) {
        totalFree += pair.second;
    }
    return totalFree < remaining;
}

void RmrsMigrateModule::PrintMigrateToNuma(const DFSContext &ctx, size_t index)
{
    // 打印一下当前结果和远端numa情况
    for (const auto &res : ctx.currentResult) {
        LOG_DEBUG << "[MemMigrate][Strategy][MigrateToNuma] Current result, index=" << index << ", pid=" << res.pid
                  << ", memSize=" << res.memSize << ", desNumaId=" << res.desNumaId << ".";
    }

    for (const auto &pair : ctx.numaFreeMap) {
        LOG_DEBUG << "[MemMigrate][Strategy][MigrateToNuma] Current remote numa, index=" << index
                  << ", nid=" << pair.first << ", freeMem=" << pair.second << ".";
    }
}

bool RmrsMigrateModule::MigrateToNuma(DFSContext &ctx, const VmInfo &vm, size_t index, uint64_t currentTotal)
{
    LOG_DEBUG << "[MemMigrate][Strategy][MigrateToNuma] Entry, index=" << index << ", currentTotal=" << currentTotal
              << ", pid=" << vm.pid << ", maxSize=" << vm.maxSize << ".";
    auto migrate = [&ctx, &vm, index, currentTotal](uint16_t nid) {
        auto it = ctx.numaFreeMap.find(nid);
        if (it == ctx.numaFreeMap.end()) {
            return false;
        }
        uint64_t &freeMem = it->second;

        LOG_DEBUG << "[MemMigrate][Strategy][MigrateToNuma] Trying, freeMem = " << freeMem << ".";
        uint64_t remaining = ctx.totalRequired - currentTotal;
        uint64_t maxAlloc = std::min({vm.maxSize, freeMem, remaining});
        freeMem -= maxAlloc;
        ctx.currentResult.push_back({vm.pid, maxAlloc, nid});
        LOG_DEBUG << "[MemMigrate][Strategy][MigrateToNuma] Trying this time, pid = " << vm.pid
                  << ", migrateMem = " << maxAlloc << ", destNuma = " << nid << ", with remainMem = " << freeMem << ".";

        PrintMigrateToNuma(ctx, index);

        if (TryMigrate(ctx, index + 1, currentTotal + maxAlloc)) {
            return true;
        }
        ctx.currentResult.pop_back();
        freeMem += maxAlloc;
        return false;
    };

    if (vm.desNumaId != 0) {
        LOG_DEBUG << "[MemMigrate][Strategy][MigrateToNuma] Remote NUMA has been bound, pid = " << vm.pid
                  << ", desNumaId = " << vm.desNumaId << ".";
        return migrate(vm.desNumaId);
    }

    std::vector<uint16_t> remoteNumaList;
    if (s_pidRemoteNumaMap.find(vm.pid) == s_pidRemoteNumaMap.end()) {
        LOG_ERROR << "[MemMigrate][Strategy][MigrateToNuma] Cannot find the remote NUMA node to the PID for migration.";
        return false;
    }

    remoteNumaList = s_pidRemoteNumaMap[vm.pid];

    for (auto nid : remoteNumaList) {
        // 如果vm.pid所属的本地numa和借来的远端numa不是同一个平面，continue
        LOG_DEBUG << "[MemMigrate][Strategy][MigrateToNuma] Checking whether VM and remote NUMA are on the same plane. "
                  << "The pid = " << vm.pid << ", vmLoacalNuma = " << vm.localNumaId << ", remoteNuma = " << nid << ".";
        if (migrate(nid)) {
            LOG_DEBUG << "[MemMigrate][Strategy][MigrateToNuma] Same plane and return true.";
            return true;
        }
    }
    return false;
}

bool RmrsMigrateModule::TryMigrate(DFSContext &ctx, size_t index, uint64_t currentTotal)
{
    if (currentTotal == ctx.totalRequired) {
        ctx.finalResult = ctx.currentResult;
        return true;
    }
    if (index >= ctx.vms.size() || CanPrune(ctx, index, currentTotal)) {
        return false;
    }
    if (MigrateToNuma(ctx, ctx.vms[index], index, currentTotal)) {
        return true;
    }
    return TryMigrate(ctx, index + 1, currentTotal);
}

bool RmrsMigrateModule::AllocateMemory(uint64_t totalSize, const std::vector<VmInfo> &vms,
                                       const std::vector<NumaInfoWithSize> &numas,
                                       std::vector<rmrs::serialization::VMMigrateOutParam> &result)
{
    DFSContext ctx(vms, numas, totalSize);
    if (TryMigrate(ctx, 0, 0)) {
        result = ctx.finalResult;
        return true;
    }
    return false;
}

// 排序规则
bool RmrsMigrateModule::VmComparator(const VmInfo &a, const VmInfo &b)
{
    // desNumaId == 0 的放到后面
    bool aIsZero = (a.desNumaId == 0);
    bool bIsZero = (b.desNumaId == 0);
    if (aIsZero != bIsZero) {
        return !aIsZero; // a 如果不是 0，它排前面
    }

    // 同为 desNumaId != 0 或都为 0，再比较 maxSize（降序）
    if (a.maxSize != b.maxSize) {
        return a.maxSize > b.maxSize;
    }

    // 如果 maxSize 相同，按 desNumaId 升序
    return a.desNumaId < b.desNumaId;
}

// 排序规则
bool RmrsMigrateModule::NumaComparator(const NumaInfoWithSize &a, const NumaInfoWithSize &b)
{
    return a.remoteNumaId < b.remoteNumaId;
}

RmrsResult RmrsMigrateModule::FillVmNumaInfo(const NumaQueryInfo &numaQueryInfo, const VMQueryInfo &vmQueryInfo,
                                             std::vector<VmInfo> &vms, std::vector<NumaInfoWithSize> &numas)
{
    for (const auto &it : vmQueryInfo.vmNumaInfoMap) {
        const auto &info = it.second;
        LOG_DEBUG << "[MemMigrate][Strategy] DFS input of map, pid = " << info.pid
                  << ", localNumaId = " << info.localNumaId << ", remoteNumaId = " << info.remoteNumaId
                  << ", localUsedMem = " << info.localUsedMem << ", remoteUsedMem = " << info.remoteUsedMem << ".";
    }

    for (const auto &pair : vmQueryInfo.vmMigratableMemMap) {
        pid_t pid = pair.first;
        uint64_t mem = pair.second / memoryThreshold;
        uint16_t desNumaId = 0;
        uint16_t localNumaId = 0;

        auto it = vmQueryInfo.vmNumaInfoMap.find(pid);
        if (it != vmQueryInfo.vmNumaInfoMap.end() && it->second.remoteUsedMem > 0) {
            desNumaId = it->second.remoteNumaId;
            localNumaId = it->second.localNumaId;
        } else if (it != vmQueryInfo.vmNumaInfoMap.end()) {
            localNumaId = it->second.localNumaId;
        } else {
            LOG_DEBUG << "[MemMigrate][Strategy] Can not find pid=" << pid << ".";
            LOG_ERROR << "[MemMigrate][Strategy] Can not find pid in map.";
            return RMRS_ERROR;
        }
        vms.push_back({pid, mem, desNumaId, localNumaId});
    }
    std::sort(vms.begin(), vms.end(), VmComparator);

    for (const auto &vm : vms) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[MemMigrate][Strategy] DFS input of vms, pid = " << vm.pid << ", maxSize = " << vm.maxSize
            << ", destNuma = " << vm.desNumaId << " , localNumaId = " << vm.localNumaId << ".";
    }

    for (size_t i = 0; i < numaQueryInfo.remoteNumaIdList.size(); i++) {
        uint16_t remoteNumaId = numaQueryInfo.remoteNumaIdList[i];
        uint64_t freeSize = 0;
        auto it = numaQueryInfo.numaInfoMap.find(remoteNumaId);
        if (it != numaQueryInfo.numaInfoMap.end()) {
            freeSize = it->second.hugePageFree;
        }
        numas.push_back({remoteNumaId, freeSize});
    }
    std::sort(numas.begin(), numas.end(), NumaComparator);

    for (const auto &numa : numas) {
        LOG_DEBUG << "[MemMigrate][Strategy] DFS input of numas, remoteNumaId = " << numa.remoteNumaId
                  << ", freeSize = " << numa.freeSize << ".";
    }
    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::ConvertMemToKB(const VMQueryInfo &vmQueryInfo,
                                             std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam)
{
    for (size_t i = 0; i < vmMigrateOutParam.size(); i++) {
        pid_t pid = vmMigrateOutParam[i].pid;
        uint64_t remoteUsedMem = 0;
        auto it = vmQueryInfo.vmNumaInfoMap.find(pid);
        if (it != vmQueryInfo.vmNumaInfoMap.end() && it->second.remoteUsedMem > 0) {
            remoteUsedMem = it->second.remoteUsedMem;
        }

        // 判断乘法是否会溢出
        if (memoryThreshold != 0 &&
            vmMigrateOutParam[i].memSize > std::numeric_limits<uint64_t>::max() / memoryThreshold) {
            LOG_ERROR << "[MemMigrate][Strategy] Overflow in memSize multiply by threshold.";
            return RMRS_ERROR;
        }

        // 判断加法是否会溢出
        if ((vmMigrateOutParam[i].memSize * memoryThreshold) > std::numeric_limits<uint64_t>::max() - remoteUsedMem) {
            LOG_ERROR << "[MemMigrate][Strategy] Unsigned integer overflow detected.";
            return RMRS_ERROR;
        }

        vmMigrateOutParam[i].memSize = vmMigrateOutParam[i].memSize * memoryThreshold + remoteUsedMem;
    }
    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::DFSGetMigrationActions(
    const uint64_t &memMigrateTotalSize, const NumaQueryInfo &numaQueryInfo, const VMQueryInfo &vmQueryInfo,
    std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam)
{
    if (memMigrateTotalSize > std::numeric_limits<uint64_t>::max() - (memoryThreshold - 1)) {
        LOG_ERROR << "[MemMigrate][Strategy] Unsigned integer overflow detected.";
        return false;
    }

    uint64_t totalSize = (memMigrateTotalSize + memoryThreshold - 1) / memoryThreshold;
    std::vector<VmInfo> vms;
    std::vector<NumaInfoWithSize> numas;

    LOG_DEBUG << "[MemMigrate][Strategy] DFS input, totalSize = " << totalSize << ".";

    // 填充参数
    RmrsResult ret = FillVmNumaInfo(numaQueryInfo, vmQueryInfo, vms, numas);
    if (ret != RMRS_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to fill vm and numa info.";
        return RMRS_ERROR;
    }

    if (AllocateMemory(totalSize, vms, numas, vmMigrateOutParam)) {
        // 将结果内存单位由页面数量转为kb，并且加上原来已经迁出的内存
        RmrsResult ret = ConvertMemToKB(vmQueryInfo, vmMigrateOutParam);
        if (ret != RMRS_OK) {
            LOG_ERROR << "[MemMigrate][Strategy] Failed to convert memory unit.";
            return RMRS_ERROR;
        }
        for (const auto &r : vmMigrateOutParam) {
            LOG_DEBUG << "[MemMigrate][Strategy] Result, pid = " << r.pid << ", size = " << r.memSize
                      << " kb, numa = " << r.desNumaId << ".";
        }
    } else {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to apply strategy.";
        return RMRS_ERROR;
    }
    LOG_INFO << "[MemMigrate][Strategy] Applying strategy success.";
    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::SetBorrowPlaneParam(const std::map<pid_t, std::vector<uint16_t>> &pidRemoteNumaMap,
                                                  const std::vector<uint16_t> timeOutNumas)
{
    // 清空原有数据
    s_pidRemoteNumaMap.clear();
    s_timeOutNumas.clear();
 
    // 赋值
    s_pidRemoteNumaMap = pidRemoteNumaMap;
    s_timeOutNumas = timeOutNumas;
 
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[MemMigrate][Strategy] The s_timeOutNumas size=" << s_timeOutNumas.size() << ".";
    for (const auto &numa : s_timeOutNumas) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[MemMigrate][Strategy] TimeOut numa =" << numa << ".";
    }

    for (const auto &[pid, numaList] : s_pidRemoteNumaMap) {
        std::ostringstream oss;
        for (size_t i = 0; i < numaList.size(); ++i) {
            oss << numaList[i];
            if (i != numaList.size() - 1) {
                oss << ",";
            }
        }
        LOG_DEBUG << "[MemMigrate][Strategy] Param pid=" << pid << " numaList=[" << oss.str() << "]";
    }

    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::CheckTimeOutNuma(const std::vector<NumaHugePageInfo> &numaHugePageInfoSumList)
{
    LOG_DEBUG << "[MemMigrate][Strategy] CheckTimeOutNuma start.";
    std::unordered_map<uint16_t, NumaHugePageInfo> infoMap;
    infoMap.reserve(numaHugePageInfoSumList.size());
    for (const auto &info : numaHugePageInfoSumList) {
        infoMap.emplace(info.numaId, info);
    }

    // 逐一检查 s_timeOutNumas
    for (uint16_t nid : s_timeOutNumas) {
        auto it = infoMap.find(nid);
        if (it == infoMap.end()) {
            LOG_ERROR << "[MemMigrate][Strategy] NumaId not found in remote list.";
            LOG_DEBUG << "[MemMigrate][Strategy] NumaId not found in remote list, numaId=" << nid << ".";
            return RMRS_ERROR;
        }

        const auto &info = it->second;
        if (info.isLocal || !info.isRemote) {
            LOG_ERROR << "[MemMigrate][Strategy] NumaId Numa state invalid.";
            LOG_DEBUG << "[MemMigrate][Strategy] Param numaId=" << nid << "is local numaId.";
            return RMRS_ERROR;
        }
    }
    LOG_DEBUG << "[MemMigrate][Strategy] CheckTimeOutNuma end.";

    return RMRS_OK;
}

RmrsResult RmrsMigrateModule::RemoveVmOfTimeOutNuma(const std::map<pid_t, VmNumaInfo> &vmNumaInfoMap,
                                                    std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam)
{
    std::vector<rmrs::serialization::VMPresetParam> keepList;

    for (const auto &param : vmPresetParam) {
        auto it = vmNumaInfoMap.find(param.pid);
        if (it == vmNumaInfoMap.end()) {
            // vmNumaInfoMap 里没有这个 pid
            LOG_ERROR << "[MemMigrate][Strategy] The vmNumaInfoMap has no pid.";
            LOG_DEBUG << "[MemMigrate][Strategy] The vmNumaInfoMap has no pid = " << param.pid << ".";
            return RMRS_ERROR;
        }

        const VmNumaInfo &info = it->second;

        // 判断 remoteNumaId 是否在 s_timeOutNumas 里
        bool isTimeOut = false;
        for (auto numaId : s_timeOutNumas) {
            if (info.remoteNumaId == numaId) {
                isTimeOut = true;
                break;
            }
        }

        if (!isTimeOut) {
            // 如果不是超时 numa，则保留
            keepList.push_back(param);
        } else {
            // 是超时 numa -> 删除，不放入 keepList
            LOG_DEBUG << "[MemMigrate][Strategy] Remove pid=" << param.pid << " due to timeout numa.";
        }
    }

    // 用保留下来的覆盖原始 vector
    vmPresetParam.swap(keepList);
    return RMRS_OK;
}

#undef LOG_DEBUG
#undef LOG_ERROR
#undef LOG_INFO

} // namespace rmrs::migrate
