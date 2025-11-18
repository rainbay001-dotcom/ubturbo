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
struct PidInfo {
    pid_t pid{}; // vm对应pid
    uint64_t localUsedMem{};
    std::vector<uint16_t> localNumaIds{}; // 本地numaId列表,只有远端使用内存>0才有效
    uint64_t remoteUsedMem{};             // 远端使用内存
    uint16_t remoteNumaId{};              // 远端numaId,只有远端使用内存>0才有效
    int socketId{};                       // pid 所在的socketId
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "pid:" << pid << ",";
        oss << "localUsedMem:" << localUsedMem << "BYTE,";
        oss << "localNumaIds:[";
        for (size_t i = 0; i < localNumaIds.size(); ++i) {
            oss << localNumaIds[i];
            if (i != localNumaIds.size() - 1) {
                oss << ", ";
            }
        }
        oss << "],";
        oss << "remoteUsedMem:" << remoteUsedMem << "BYTE,";
        oss << "remoteNumaId:" << remoteNumaId << ",";
        oss << "socketId:" << socketId;
        oss << "}";
        return oss.str();
    }
};

} // namespace mempooling

#endif // MP_RES_QUERY_H
