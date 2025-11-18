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
#ifndef RMRS_VM_INFO_H
#define RMRS_VM_INFO_H

#include <sstream>
#include <string>
#include <vector>

namespace rmrs {

struct VmMetaData {
    VmMetaData() = default;

    std::string nodeId{};     // 物理节点id-管控面配置文件
    std::string hostName{};   // 物理节点hostName-虚机xml定义文件
    std::string uuid{};       // 虚拟机uuid-虚机xml定义文件
    std::string name{};       // 虚拟机name-虚机xml定义文件
    int16_t socketId{};       // cpu对应socketID
    int16_t numaId{};         // cpu对应的NumaId
    uint64_t cpuTime{};       // 虚机的cpu耗时
    bool isProportionally;    // 虚机是否按比例创建-虚机xml定义文件-预留字段未采集未使用
    time_t vmCreateTime{};    // 虚机创建时间-libvirt采集
    uint16_t cpuNums{};       // 虚机CPU核数-虚机xml定义文件
    uint64_t maxMem{};        // 虚机申请内存-虚机xml定义文件，单位KBytes
    uint64_t usedMem{};       // 虚机已使用内存-虚机xml定义文件，单位KBytes
    pid_t pid{};              // 虚机进程PID-操作系统提供
    uint64_t pageSize{}; // 虚机页大小-默认2mBytes大页，即2048KBytes

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "nodeId=" << nodeId << ",";
        oss << "hostName=" << hostName << ",";
        oss << "uuid=" << uuid << ",";
        oss << "name=" << name << ",";
        oss << "socketId=" << socketId << ",";
        oss << "numaId=" << numaId << ",";
        oss << "cpuTime=" << cpuTime << ",";
        oss << "isProportionally=" << isProportionally << ",";
        oss << "vmCreateTime=" << vmCreateTime << ",";
        oss << "cpuNums=" << cpuNums << ",";
        oss << "maxMem=" << maxMem << ",";
        oss << "usedMem=" << usedMem << ",";
        oss << "pid=" << pid << ",";
        oss << "pageSize=" << pageSize;
        oss << "}";
        return oss.str();
    }
};

struct VmCpuInfo {
    VmCpuInfo() = default;

    uint16_t vCpuId{};    // 映射虚拟cpu核Id, libvirt采集-libvirt定义
    uint16_t cpuId{};     // 映射物理cpu核Id, libvirt采集-可以xml定义也可以让libvirt自由分配
    uint16_t cpuNumaId{}; // 物理cpu核所在numaid, 系统文件采集
    time_t cpuTime{};     // cpu时间，系统采集
    uint16_t socketId{};  // cpu对应的socketId

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "vCpuId=" << vCpuId << ",";
        oss << "cpuId=" << cpuId << ",";
        oss << "cpuNumaId=" << cpuNumaId << ",";
        oss << "cpuTime=" << cpuTime << ",";
        oss << "socketId=" << socketId;
        oss << "}";
        return oss.str();
    }
};

struct VmDomainInfo {
    VmDomainInfo() = default;

    VmMetaData metaData{};             // 元信息
    uint16_t localNumaId;              // 本地numaId
    uint64_t localUsedMem;             // 本地使用内存
    uint16_t remoteNumaId;             // 远端numaId
    uint64_t remoteUsedMem;            // 远端使用内存
    std::vector<VmCpuInfo> cpuInfos{}; // cpu信息
    std::string state{};               // 虚机状态
    time_t timestamp{};                // 时间戳

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "metaData=[" << metaData.toString() << "],";
        oss << "localNumaId=" << localNumaId << ",";
        oss << "localUsedMem=" << localUsedMem << ",";
        oss << "remoteNumaId=" << remoteNumaId << ",";
        oss << "remoteUsedMem=" << remoteUsedMem << ",";
        oss << "cpuInfos=[";
        for (const auto &cpuInfo : cpuInfos) {
            oss << cpuInfo.toString() << ",";
        }
        oss << "],";
        oss << "state=" << state;
        oss << "}";
        return oss.str();
    }
};

struct HostVmDomainInfo {
    HostVmDomainInfo() = default;

    std::string nodeId{};
    std::string hostName{};
    std::string type = "metric";
    std::vector<VmDomainInfo> vmDomainInfos{};

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "nodeId=" << nodeId << ",";
        oss << "hostName=" << hostName << ",";
        oss << "vmDomainInfos=[";
        for (const auto &vmDomainInfo : vmDomainInfos) {
            oss << vmDomainInfo.toString() << ",";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

} // namespace rmrs

#endif // RMRS_VM_INFO_H