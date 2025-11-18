/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef RMRS_ROLLBACK_MODULE_H
#define RMRS_ROLLBACK_MODULE_H

#include "rmrs_migrate_module.h"

namespace rmrs::migrate {

class RmrsRollbackModule {
public:
    static uint32_t HandlerRollback(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);

private:
    static uint32_t MemBorrowRollback(std::map<std::string,
        std::set<rmrs::serialization::BorrowIdInfo>> &borrowIdsPidsMap);
    static bool FillRollbackVmInfo(std::map<pid_t, VmDomainInfo> &vmInfoMap, std::vector<pid_t> &pidList,
        std::unordered_map<pid_t, uint16_t> &vmPidRemoteNumaMap, std::vector<uint16_t> &remoteNumaIdList);
    static bool CanMigrate(std::map<pid_t, VmDomainInfo> &vmInfoMap,
                           const std::set<rmrs::serialization::BorrowIdInfo> &pidInfoList);
    static bool CanMigrateBack(std::map<pid_t, VmDomainInfo> &vmInfoMap, std::map<uint16_t, NumaInfo> &numaInfoMap,
                               const std::set<rmrs::serialization::BorrowIdInfo> &pidInfoList);
    static bool DoMigrateRollback(std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap,
                                  const std::set<rmrs::serialization::BorrowIdInfo> &pidInfoList);
}; // namespace rmrs::migrate

}

#endif