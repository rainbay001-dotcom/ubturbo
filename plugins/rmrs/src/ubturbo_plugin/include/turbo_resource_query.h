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
#ifndef MP_RES_QUERY_H
#define MP_RES_QUERY_H

#include <vector>

#include "numa_info.h"
#include "vm_info.h"
#include "rmrs_serialize.h"

namespace mempooling {
using namespace rmrs::serialize;
struct MetaNumaInfo {
    uint16_t numaId{};       // numaId
    uint64_t numaUsedMem{};  // 该numaId上使用的内存
    bool isLocalNuma{true};  // 是否本地numa
    int socketId{-1};        // numaId对应的socketId

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numaId:" << numaId << ",";
        oss << "numaUsedMem:" << numaUsedMem << " BYTE,";
        oss << "isLocalNuma:" << isLocalNuma << ",";
        oss << "socketId:" << socketId;
        oss << "}";
        return oss.str();
    }
};

struct PidInfo {
    pid_t pid{};                                 // 进程id
    std::vector<uint16_t> localNumaIds{};        // 本地numaId集合
    uint64_t totalLocalUsedMem{};                // 本地numa上使用的总内存大小
    uint64_t totalRemoteUsedMem{};               // 远端numa使用的总内存大小
    std::vector<MetaNumaInfo> metaNumaInfos{};   // pid进程元信息集合
    
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "pid:" << pid;
        oss << ",localNumaIds:[";
        for (size_t i = 0; i < localNumaIds.size(); ++i) {
            if (i) oss << ", ";
            oss << localNumaIds[i];
        }
        oss << "]";
        oss << "totalLocalUsedMem:" << totalLocalUsedMem << "BYTE";
        oss << "totalRemoteUsedMem:" << totalRemoteUsedMem << "BYTE";
        oss << ",metaNumaInfos:[";
        for (size_t i = 0; i < metaNumaInfos.size(); ++i) {
            if (i) oss << ", ";
            oss << metaNumaInfos[i].ToString();
        } 
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

} // namespace mempooling

#endif // MP_RES_QUERY_H
