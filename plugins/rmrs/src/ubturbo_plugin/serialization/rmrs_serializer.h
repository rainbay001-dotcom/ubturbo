/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */#ifndef RMRS_SERIALIZER_H
#define RMRS_SERIALIZER_H

#include <string>
#include <vector>

#include "turbo_resource_query.h"
#include "rmrs_serialize.h"

namespace rmrs::serialization {
using namespace rmrs::serialize;

constexpr int SERIALIZER_OK = 0;
constexpr int SERIALIZER_ERROR = 1;

struct VMPresetParam {
    pid_t pid;      // vm对应pid
    uint16_t ratio; // 迁出最大比例
};

struct VMMigrateOutParam {
    pid_t pid;
    uint64_t memSize;   // 迁出内存大小 单位kb
    uint16_t desNumaId; // 迁移远端numa
};

struct VMMigrateMultiOutParam {
    pid_t pid;
    std::vector<uint64_t> memSize;  // 迁出内存大小 单位kb
    std::vector<uint16_t> desNumaId; // 迁移远端numa
};

class MemBorrowRollbackParam {
public:
    std::string nodeId{};
    std::vector<std::string> borrowIdsList{};
};

class MigrateExecuteParam {
public:
    std::vector<VMMigrateOutParam> vmInfoList{};
    uint64_t waitingTime{};
    std::vector<std::string> borrowIdList{};
};

class MigrateBackResult {
public:
    uint32_t result{};
    std::vector<uint16_t> numaIds{};
};

/**
 * 外部模块需要的拓扑中各节点信息
 * 拓扑以socket为单位
 */
struct NumaData {
    std::string numaId{};
    bool operator==(const NumaData &numaData) const
    {
        return numaId == numaData.numaId;
    }
};

struct RemoteNumaSocketInfo {
    int16_t borrowRemoteNuma{-1}; // 借入numa, remote 借用时有效，否则为-1
    std::string lentNode{};      // 借出节点
    uint16_t lentSocketId{0};    // 借出内存socketId
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "borrow_Remote_Numa=" << borrowRemoteNuma;
        oss << ", lentNode=" << lentNode;
        oss << ", lent_Socket_Id=" << lentSocketId << ",";
        oss << "}";
        return oss.str();
    }
};

class MigrateStrategyParamRMRS {
public:
    std::vector<VMPresetParam> vmInfoList{};
    std::uint64_t borrowSize{};
    std::map<pid_t, std::vector<uint16_t>> pidRemoteNumaMap;       // 需要匀出本地内存大小
    std::vector<uint16_t> timeOutNumas;                            // 归还超时的远端numa
};

class MigrateStrategyResult {
public:
    std::vector<VMMigrateOutParam> vmInfoList{};
    uint64_t waitingTime{}; // 单位ms
};

class FaultMemIdManageParam {
public:
    std::string importNodeId{};
    uint64_t importMemId{};
};

class PidNumaInfoCollectParam {
public:
    std::vector<pid_t> pidList{};
};

class PidNumaInfoCollectResult {
public:
    std::vector<mempooling::PidInfo> pidInfoList{};
};

class NumaMemInfoCollectParam {
public:
    int numaId{};
};

} // namespace rmrs::serialization

#endif