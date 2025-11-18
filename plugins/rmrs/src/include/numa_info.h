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
#ifndef NUMA_INFO_H
#define NUMA_INFO_H

#include <sstream>
#include <string>
#include <vector>

namespace mempooling {

struct NumaMetaData {
    NumaMetaData() = default;

    bool operator==(const NumaMetaData &numaMetaData) const
    {
        if (this->nodeId == numaMetaData.nodeId && this->numaId == numaMetaData.numaId &&
            this->socketId == numaMetaData.socketId) {
            return true;
        }
        return false;
    }

    std::string nodeId{}; // 节点Id
    uint32_t logicId{};
    int16_t logicSocketId{};
    std::string hostName{};         // 节点HostName
    uint16_t numaId{};              // numaId
    bool isLocal;                   // 是否是本地numa
    bool isRemoteAvailable = true;  // 远端numa是否可用于内存迁移
    uint64_t memTotal{};            // 该numa节点内存总量(包含)，系统文件采集，kb
    uint64_t memFree{};             // 该numa上空闲内存，系统文件采集，kb
    uint64_t hugePageTotal{};       // 该numa上大页数量
    uint64_t hugePageFree{};        // 该numa上空闲的大页数量
    uint16_t cpuCounts{};           // 该numa上cpu核数
    uint16_t socketId{};            // 该numa绑定cpu映射socketId
    std::vector<uint16_t> cpuIds{}; // 该numa绑定cpu列表

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "nodeId=" << nodeId << ",";
        oss << "logicId=" << logicId << ",";
        oss << "logicSocketId=" << logicSocketId << ",";
        oss << "hostName=" << hostName << ",";
        oss << "numaId=" << numaId << ",";
        oss << "isLocal=" << isLocal << ",";
        oss << "isRemoteAvailable=" << isRemoteAvailable << ",";
        oss << "memTotal=" << memTotal << ",";
        oss << "memFree=" << memFree << ",";
        oss << "hugePageTotal=" << hugePageTotal << ",";
        oss << "hugePageFree=" << hugePageFree << ",";
        oss << "cpuCounts=" << cpuCounts << ",";
        oss << "socketId=" << socketId << ",";
        oss << "cpuIds=[";
        for (const auto &cpuId : cpuIds) {
            oss << cpuId << ", ";
        }
        oss << "],";
        oss << "}";
        return oss.str();
    }
};

struct NumaVmInfo {
    NumaVmInfo() = default;

    uint64_t vmTotalMaxMem{};            // numa上虚拟机申请内存总量
    uint64_t vmTotalUsedMem{};           // numa上虚拟机使用内存总量
    uint16_t vmTotalAllocatedCpuNum{};   // numa上虚拟机绑定的cpu总数
    std::vector<uint16_t> vmCpuList{};   // numa上虚拟机绑定cpu列表
    std::vector<uint16_t> freeCpuList{}; // numa上没有被虚拟机绑定的cpu列表
    std::vector<pid_t> vmsOnNuma{};      // 该numa上运行的vm列表（以mem>为准, 存虚机pid）

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "vmTotalMaxMem=" << vmTotalMaxMem << ",";
        oss << "vmTotalUsedMem=" << vmTotalUsedMem << ",";
        oss << "vmTotalAllocatedCpuNum=" << vmTotalAllocatedCpuNum << ",";
        oss << "vmCpuList=[";
        for (const auto &cpuId : vmCpuList) {
            oss << cpuId << ", ";
        }
        oss << "],";
        oss << "freeCpuList=[";
        for (const auto &cpuId : freeCpuList) {
            oss << cpuId << ", ";
        }
        oss << "]";
        oss << "vmsOnNuma=[";
        for (const auto &pid : vmsOnNuma) {
            oss << pid << ", ";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct NumaInfo {
    NumaInfo() = default;

    NumaMetaData numaMetaInfo{}; // numa元信息
    NumaVmInfo numaVmInfo{};     // numa上虚拟机相关信息

    bool operator==(const NumaInfo &numaInfo) const
    {
        if (this->numaMetaInfo == numaInfo.numaMetaInfo) {
            return true;
        }
        return false;
    }

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numaMetaInfo=[" << numaMetaInfo.toString() << "],";
        oss << "numaVmInfo=[" << numaVmInfo.toString() << "]";
        oss << "}";
        return oss.str();
    }
};

struct NodeInfo {
    NodeInfo() = default;

    std::string nodeId{};
    std::string hostName{};
    std::vector<uint16_t> cpuIds{};     // 该node上cpu列表
    std::vector<uint16_t> freeCPUIds{}; // 该node上空闲cpu列表
    std::vector<NumaInfo> numaInfos{};  // 该node上numa信息，包含numa元信息和与虚机相关的信息

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "nodeId=" << nodeId << ",";
        oss << "hostName=" << hostName << ",";
        oss << "freeCpuList=[";
        for (const auto &cpuId : freeCPUIds) {
            oss << cpuId << ", ";
        }
        oss << "],";
        oss << "vmsOnNuma=[";
        for (const auto &numaInfo : numaInfos) {
            oss << numaInfo.toString() << ", ";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct NumaParam {
    NumaParam() = default;

    std::string hostName;
    uint16_t numaId;
    bool isLocal;
    uint64_t memTotal;
    uint64_t memFree;
    uint64_t hugePageTotal;
    uint64_t hugePageFree;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "hostName=" << hostName << ",";
        oss << "isLocal=" << isLocal << ",";
        oss << "memTotal=" << memTotal << ",";
        oss << "memFree=" << memFree << ",";
        oss << "hugePageTotal=" << hugePageTotal << ",";
        oss << "hugePageFree=" << hugePageFree;
        oss << "}";
        return oss.str();
    }
};

struct NumaSizeInfo {
    NumaSizeInfo() = default;

    time_t timestamp;
    std::string nodeId;
    std::vector<NumaParam> numaParams;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "timestamp=" << timestamp << ",";
        oss << "nodeId=" << nodeId << ",";
        oss << "numaParams=[";
        for (const auto &numaParam : numaParams) {
            oss << numaParam.ToString() << ", ";
        }
        oss << "],";
        oss << "}";
        return oss.str();
    }
};

} // namespace mempooling

#endif // NUMA_INFO_H
