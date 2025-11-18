/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
 
#ifndef RMRS_MEMFREE_MODULE_H
#define RMRS_MEMFREE_MODULE_H
 
#include "rmrs_migrate_module.h"
 
namespace rmrs::migrate {
 
class RmrsMemFreeModule {
public:
    static RmrsResult MemFreeImpl(std::vector<uint16_t> &numaIds);
    static void ResolveReturnNumaIds(std::vector<NumaInfo> numaInfos, std::vector<uint16_t> &numaIds);
    static void DistributeNumaInfo(std::vector<NumaInfo> &numaInfos, std::vector<ReturnNumaInfo> &returnNumaInfoSumList,
        std::map<uint16_t, int> &localNumaMemFreeMap);
    static void SortRemoteNumaByMemUse(std::vector<ReturnNumaInfo> &returnNumaInfoSumList,
                                       std::vector<uint16_t> &remoteNumaIdList);
    static bool ReturnNumaInfoComparator(ReturnNumaInfo &info1, ReturnNumaInfo &info2);
    static std::vector<ReturnVmInfo> GetCanReturnVmInfo(std::map<uint16_t, int> localNumaMemFreeMap,
                                                        std::vector<uint16_t> remoteNumaIdList,
                                                        std::vector<VmDomainInfo> &vmDomainInfos);
    static RmrsResult ReturnRemoteNuma(std::vector<ReturnVmInfo> &canReturnVmInfoList, std::vector<uint16_t> &numaIds);
 
private:
    static bool GetExceptPidList(std::vector<rmrs::smap::ProcessPayload> processPayloadList, std::vector<pid_t> pidList,
                                 std::vector<pid_t> &exceptPidList);
}; // namespace rmrs::migrate
 
}
 
#endif