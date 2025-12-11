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
#ifndef RMRS_MIGRATE_MODULE_H
#define RMRS_MIGRATE_MODULE_H

#include <sys/types.h>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "rmrs_error.h"
#include "rmrs_vm_info.h"
#include "rmrs_numa_info.h"
#include "turbo_def.h"
#include "rmrs_serializer.h"
#include "rmrs_json_helper.h"
#include "rmrs_smap_module.h"

namespace rmrs::migrate {

const std::uint64_t MIGRATEOUT_TIMEOUT = 50 * 1000 + 1;

struct ReturnNumaInfo {
    uint16_t numaId{};
    uint64_t memFree{};
    uint64_t memUse{};
    uint64_t hugePageFree{};
    uint64_t hugePageUse{};
    bool isLocal;
};

struct ReturnVmInfo {
    pid_t pid;
    uint16_t remoteNumaId;
};

struct VmNumaInfo {
    pid_t pid;
    uint16_t localNumaId;
    uint16_t remoteNumaId;
    uint64_t localUsedMem;
    uint64_t remoteUsedMem;
};

struct NumaHugePageInfo {
    uint16_t numaId{};
    uint64_t hugePageTotal{};
    uint64_t hugePageFree{};
    bool isLocal;
    bool isRemote;
    bool isTimeOut;
};

struct NumaMemStatus {
    std::vector<uint16_t> remoteNumaIdList;                    // 远端 NUMA 节点列表
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;          // NUMA 节点对应的内存信息
};

struct VmMigrationContext {
    std::map<pid_t, uint64_t> vmMigratableMemMap;            // 虚拟机潜在可迁出的内存量
    std::map<uint16_t, std::vector<pid_t>> remoteNumaPidMap; // 每个远端 NUMA 节点对应的虚拟机列表
    std::vector<pid_t> localNumaPidVec;                      // 完全本地 NUMA 分布的虚拟机列表
    std::vector<pid_t> vmHotnessPriorityVec;                 // 虚拟机热度排序（迁出优先级）
    std::map<pid_t, VmNumaInfo> vmNumaInfoMap;               // 虚拟机使用内存情况
};

struct MemSizeStats {
    double targetMemSize;  // 目标内存大小
    double memSizeSum;     // 当前累计的内存大小
};

struct SecondaryMigrateParam {
    uint64_t memMigrateAbleTmp;   // 本次可迁出的内存
    double remainingMigrateMem;   // 还需迁出的内存（double 表示更精确）
    uint16_t remoteID;            // 远端 NUMA 节点 ID
    uint64_t remoteMemMigrated;   // 当前远端已经迁出的内存
    pid_t pid;                    // 虚拟机 PID
    uint64_t remoteFreeMem;       // 远端NUMA内存空闲
};

struct NumaQueryInfo {
    std::vector<rmrs::NumaInfo> numaInfos;                         // 当前节点 NUMA 信息
    std::vector<uint16_t> remoteNumaIdList;                        // 远端 NUMA 节点 ID 列表
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;             // 远端 NUMA 节点 ID -> 对应大页信息
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList;        // 所有远端节点大页统计信息
};

struct VMQueryInfo {
    std::vector<rmrs::VmDomainInfo> vmDomainInfos;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfo> vmNumaInfoMap;
    std::map<pid_t, uint64_t> vmMigratableMemMap;
};

struct SmapVmHotnessQueryParam {
    std::vector<pid_t> migrateVMPids;                                 // 虚拟机纳入 SMAP 管理的 PID 列表
    std::map<pid_t, std::vector<uint16_t>> vmFreqMap;                 // 每个 VM 的冷热频次数组
    std::map<pid_t, uint16_t> vmFreqPidRatioMap;                      // 每个 VM 的迁出比例
    std::vector<pid_t> localNumaPidMap;                               // 属于本地 NUMA 的 VM PID 列表
    std::map<uint16_t, std::vector<pid_t>> remoteNumaPidMap;          // 每个远端 NUMA 节点对应的 VM PID 列表
    uint16_t remoteNumaId = 0;                                        // 当前迁出目标的远端 NUMA 节点 ID
    std::vector<pid_t> vmHotnessPriorityVec;                          // 冷热优先级排列的 VM PID 列表（冷的在前）
};

struct VmInfo {
    pid_t pid;
    uint64_t maxSize;
    uint16_t desNumaId;
    uint16_t localNumaId;
};

struct NumaInfoWithSize {
    uint16_t remoteNumaId;
    uint64_t freeSize;
};

struct DFSContext {
    const std::vector<VmInfo> &vms;
    std::unordered_map<uint16_t, uint64_t> numaFreeMap;
    uint64_t totalRequired;
    std::vector<rmrs::serialization::VMMigrateOutParam> currentResult;
    std::vector<rmrs::serialization::VMMigrateOutParam> finalResult;

    DFSContext(const std::vector<VmInfo> &v, const std::vector<NumaInfoWithSize> &n, uint64_t t)
        : vms(v),
          totalRequired(t)
    {
        for (const auto &numa : n) {
            numaFreeMap[numa.remoteNumaId] = numa.freeSize;
        }
    }
};

class NoexceptString {
public:
    NoexceptString() noexcept = default;
    explicit NoexceptString(const std::string &str) noexcept : data_(str) {}
    explicit NoexceptString(const char *str) noexcept : data_(str) {}
 
    // 允许从 std::string 赋值
    NoexceptString &operator=(const std::string &str) noexcept
    {
        data_ = str;
        return *this;
    }
 
    // 允许从 const char* 赋值
    NoexceptString &operator=(const char *str) noexcept
    {
        data_ = str;
        return *this;
    }
 
    // 转换为 const std::string&，不会抛异常
    operator const std::string &() const noexcept
    {
        return data_;
    }
 
    void Clear() noexcept
    {
        data_.clear();
    }
 
    const std::string &Str() const noexcept
    {
        return data_;
    }
 
private:
    std::string data_;
};

class RmrsMigrateModule {
public:
    static RmrsResult MigrateStrategyRmrs(const uint64_t &memMigrateTotalSize,
                                          std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
                                          std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam,
                                          uint32_t &waitingTime);

    static bool ISPresetMemorySufficient(const uint64_t &memMigrateTotalSize,
                                         const std::map<pid_t, VmNumaInfo> &vmNumaInfoMap,
                                         const std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
                                         std::map<pid_t, uint64_t> &vmMigratableMemMap);

    static void GetMigrateOutTime(const uint64_t &memMigrateTotalSize, uint32_t &waitingTime);

    static bool ISRemoteMemorySufficient(std::vector<uint16_t> &remoteNumaIdList,
                                         std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                                         const uint64_t &memMigrateTotalSize);

    static RmrsResult DoMigrateExecute(const std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam,
        uint64_t waitingTime);
 
    static void RollbackVmMigrate(const std::vector<rmrs::serialization::VMMigrateMultiOutParam> &executedParamList,
        std::unordered_map<pid_t, uint64_t> vmOriginSizeMap);

    static RmrsResult GetNumaData(std::vector<rmrs::NumaInfo> &numaInfos, std::vector<uint16_t> &remoteNumaIdList,
                                  std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                                  std::vector<NumaHugePageInfo> &numaHugePageInfoSumList);

    static RmrsResult GetVMData(std::vector<rmrs::VmDomainInfo> &vmDomainInfos,
                                std::vector<VmNumaInfo> &allVmNumaInfoInfoList,
                                std::map<pid_t, VmNumaInfo> &vmNumaInfoMap);

    static RmrsResult CheckNumaAndVMStatus(const uint64_t &memMigrateTotalSize,
                                           std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
                                           NumaQueryInfo &numaQueryInfo, VMQueryInfo &vmQueryInfo);

    static bool CanPrune(const DFSContext &ctx, size_t index, uint64_t currentTotal);

    static bool MigrateToNuma(DFSContext &ctx, const VmInfo &vm, size_t index, uint64_t currentTotal);

    static bool TryMigrate(DFSContext &ctx, size_t index, uint64_t currentTotal);

    static bool AllocateMemory(uint64_t totalSize, const std::vector<VmInfo> &vms,
                               const std::vector<NumaInfoWithSize> &numas,
                               std::vector<rmrs::serialization::VMMigrateOutParam> &result);

    static RmrsResult DFSGetMigrationActions(const uint64_t &memMigrateTotalSize, const NumaQueryInfo &numaQueryInfo,
                                             const VMQueryInfo &vmQueryInfo,
                                             std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam);

    static bool VmComparator(const VmInfo &a, const VmInfo &b);

    static bool NumaComparator(const NumaInfoWithSize &a, const NumaInfoWithSize &b);

    static RmrsResult FillVmNumaInfo(const NumaQueryInfo &numaQueryInfo, const VMQueryInfo &vmQueryInfo,
                                     std::vector<VmInfo> &vms, std::vector<NumaInfoWithSize> &numas);

    static RmrsResult ConvertMemToKB(const VMQueryInfo &vmQueryInfo,
                                     std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam);

    static RmrsResult FillVmOriginSize(std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParamList,
        std::unordered_map<pid_t, uint64_t> &vmOriginSizeMap);

    static RmrsResult SetBorrowPlaneParam(const std::map<pid_t, std::vector<uint16_t>> &pidRemoteNumaMap,
                                          const std::vector<uint16_t> timeOutNumas);

    static void DistributeNumaMemInfo(std::vector<rmrs::NumaInfo> &numaInfos,
                                      std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                                      std::vector<NumaHugePageInfo> &numaHugePageInfoSumList);

    static RmrsResult RemoveVmOfTimeOutNuma(const std::map<pid_t, VmNumaInfo> &vmNumaInfoMap,
                                            std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam);

    static void PrintMigrateToNuma(const DFSContext &ctx, size_t index);

    static RmrsResult CheckTimeOutNuma(const std::vector<NumaHugePageInfo> &numaHugePageInfoSumList);

    static const uint64_t memoryPageSize;               // 2048
    static const uint64_t memoryThreshold;              // 10240kb;
    static const uint64_t memoryPreset;                 //  100
    static const uint64_t smapMigrateOutWaitingTimeMin; // 内存迁出最小等待时间
    static const uint64_t smapMigrateOutWaitingTimeMax; // 内存迁出最大等待时间
    static const double ratioDiff;                      // 迁移比例允许取整差值 0.01
    static const uint16_t freqNumZero;                  // 频次等于0的
    static const uint16_t freqNumOne;                   // 频次等于1的
    static const uint16_t freqNumTwo;                   // 频次等于2的
    static const uint16_t freqNumThree;                 // 频次等于3的
    static const uint16_t freqNumFour;                  // 频次等于4的
    static const uint64_t freqScoreSeven;               // 分数等于7的
    static const uint64_t freqScoreTwo;                 // 分数等于2的
    static const uint64_t freqScoreOne;                 // 分数等于1的
    static const uint32_t smapMigrateCycleTime;         // smap每个迁移周期时间 2秒 = 2000ms
    static const uint32_t smapMigrateBaseCycle;         // 1个迁移周期起步
    static const uint32_t smapMigrate32GCycle;          // 每增加32G增加3个迁移周期
    static const uint32_t smapMigrateUnitMem;           // 32G
    static const uint64_t byte2KB;                      // bite转为KB 1024单位转换
private:
    static std::vector<uint16_t> s_timeOutNumas; // 超时归还numa信息
    static std::map<pid_t, std::vector<uint16_t>> s_pidRemoteNumaMap; // pid对应的远端numa信息Map
};
} // namespace rmrs::migrate

#endif