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
#ifndef RMRS_RESSOURCE_EXPORT_H
#define RMRS_RESSOURCE_EXPORT_H
#include <unistd.h>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "map"
#include "turbo_resource_query.h"
#include "rmrs_error.h"
#include "rmrs_numa_info.h"
#include "rmrs_vm_info.h"
namespace rmrs::exports {

using namespace rmrs;
const int SOCKET_NUM = 2;

std::string ReadFileFirstLine(const std::string &pathStr);
std::vector<int> ParseCpuList(const std::string &cpuList);

bool IsPathValid(const std::filesystem::path &path);
class ResourceExport {
public:
    ResourceExport() noexcept = default;
    static RmrsResult Init();
    static RmrsResult ReInit();
    static std::string GetHostName();

    inline static std::string GetNodeId()
    {
        return nodeId;
    };

    static RmrsResult remove_elements(std::vector<uint16_t> &vecA, std::vector<uint16_t> &vecB);

    uint32_t CollectVmInfo();

    uint32_t CollectNumaInfo();

    uint32_t UpdateVmDomainInfo();

    static std::set<uint16_t> GetLocalNodeIds();

    static bool IsValidLocalNumaNode(const std::filesystem::directory_entry &entry, uint16_t &localNodeIds);

    static mempooling::PidInfo BuildPidInfoFromNodePages(pid_t pid, bool hasLibvirt,
                                                         const std::unordered_map<uint16_t, uint64_t> &nodePages,
                                                         const std::set<uint16_t> &localNodeIds);

    std::vector<VmDomainInfo> *GetVmDomainInfos();
    std::unordered_map<uint16_t, uint16_t> *GetVmNumaCpuInfos();
    std::unordered_map<uint16_t, std::vector<uint16_t>> *GetVmNumaCpuIds();
    std::unordered_map<uint16_t, uint64_t> *GetVmNumaMaxMemInfos();
    std::unordered_map<uint16_t, uint64_t> *GetVmNumaUsedMemInfos();
    static std::vector<NumaInfo> *GetNumaInfos();
    static std::unordered_map<uint16_t, uint16_t> *GetCpuSocketMap();
    static std::unordered_map<uint16_t, uint16_t> *GetCpuNumaMap();
    static std::unordered_map<uint16_t, bool> *GetNumaLocalMap();
    std::unordered_map<std::string, pid_t> *GetUuidPIDMap();
    std::unordered_map<uint16_t, std::unordered_set<pid_t>> *GetNumaVmPidMap();
    std::unordered_map<pid_t, std::string> *GetPidUUIDMap();
    std::unordered_map<std::string, uint16_t> *GetUuidNumaMap();
    std::unordered_map<pid_t, time_t> *GetPidCreatTimeMap();
    std::vector<pid_t> *GetVPidList();
    std::vector<std::string> *GetUuidList();
    std::unordered_map<std::string, uint64_t> *GetUuidPagesizeMap();
    std::unordered_map<std::string, std::string> *GetUuidNameMap();
    std::unordered_map<std::string, uint64_t> *GetRemoteUsedMem();
    std::unordered_map<std::string, uint64_t> *GetLocalUsedMem();
    std::unordered_map<std::string, uint16_t> *GetRemoteNumaId();
    std::unordered_map<std::string, uint16_t> *GetLocalNumaId();
    std::unordered_map<uint16_t, uint64_t> *GetNumaMemTotalMap();
    std::unordered_map<uint16_t, uint64_t> *GetNumaMemFreeMap();
    std::unordered_map<uint16_t, uint64_t> *GetHugePageTotalMap();
    std::unordered_map<uint16_t, uint64_t> *GetHugePageFreeMap();

    static uint32_t GetNumaInfoImmediately(std::vector<NumaInfo> &numaInfos);
    static uint32_t GetVmInfoImmediately(std::vector<VmDomainInfo> &vmDomainInfos);
    static uint32_t CollectPidNumaInfo(const std::vector<pid_t> &pids, std::vector<mempooling::PidInfo> &pidInfos);
    static uint32_t CollectMeminfobyNumaId(int numaid, std::map<std::string, std::string> &meminfo);
    static int GetNumaIdToSocketId(uint16_t numaId);
    static int GetSocketIdFromCpu(int cpuId);
    static bool HandleMemInfoFile(const int numaid, const std::filesystem::path path,
                                  std::map<std::string, std::string> &meminfo);
    static bool HandlePidNumaInfoFile(std::istringstream &iss, const pid_t pid, const std::set<uint16_t> &localNodeIds,
                                      std::vector<mempooling::PidInfo> &pidInfos);

private:
    static std::string hostName;
    static std::string nodeId;
    static std::unordered_map<uint16_t, uint16_t> cpuSocketMap;         // cpu core绑定的Socket
    static std::unordered_map<uint16_t, uint16_t> cpuNumaMap;           // cpu core绑定的NumaId
    static std::unordered_map<uint16_t, bool> numaLocalMap;             // numa是否是本地
    std::unordered_map<uint16_t, uint64_t> numaMemTotalMap;             // numa总内存表
    std::unordered_map<uint16_t, uint64_t> numaMemFreeMap;              // numa空闲内存表
    std::unordered_map<uint16_t, uint64_t> numaHugePageTotalMap;        // numa总大页表
    std::unordered_map<uint16_t, uint64_t> numaHugePageFreeMap;         // numa空闲大页表
    static std::vector<NumaInfo> numaInfos;                             // Numa信息列表
    std::vector<VmDomainInfo> VmDomainInfos{};                          // 虚拟机信息列表
    std::unordered_map<uint16_t, uint16_t> vmNumaCpuInfos{};            // Numa上绑了几个cpu core
    std::unordered_map<uint16_t, std::vector<uint16_t>> vmNumaCpuIds{}; // Numa上绑定cpu core列表
    std::unordered_map<uint16_t, uint64_t> vmNumaMaxMemInfos{}; // Numa上绑定cpu对应的虚拟机的申請内存规模之和
    std::unordered_map<uint16_t, uint64_t> vmNumaUsedMemInfos{}; // Numa上绑定cpu对应的虚拟机的使用内存规模之和
    std::unordered_map<std::string, pid_t> uuidPIDMap{};         // 虚机 uuid -- pid
    std::unordered_map<pid_t, std::string> pidUUIDMap{};         // 虚机 pid -- uuid
    std::unordered_map<std::string, uint16_t> uuidNumaMap{};     // 虚机 uuid -- numa node
    std::unordered_map<pid_t, time_t> pidCreatTimeMap{};         // 虚机 pid -- createTime
    std::unordered_map<std::string, std::string> uuidNameMap{};  // 虚机 uuid -- name
    std::unordered_map<std::string, uint64_t> uuidPagesizeMap{}; // 虚机 uuid -- pagesize
    std::vector<pid_t> vPidList{};                               // 虚机 pid列表
    std::vector<std::string> uuidList{};                         // 虚机 uuid列表
    std::unordered_map<std::string, uint16_t> remoteNumaId{};    // 虚拟机uuid使用的远端numaId
    std::unordered_map<std::string, uint16_t> localNumaId{};     // 虚拟机uuid使用的本地numaId
    std::unordered_map<std::string, uint64_t> remoteUsedMem{};   // 虚拟机uuid使用的远端内存量
    std::unordered_map<std::string, uint64_t> localUsedMem{};    // 虚拟机uuid使用的本地内存量
    std::unordered_map<uint16_t, std::unordered_set<pid_t>>
        numaVmPidMap{}; // numa上使用了该numa的vm的pid列表 numaId - pid_t
    static std::string numaNodePathHead;
    static std::string cpuSocketPathPrefix;
    static std::string cpuSocketPath;
};
} // namespace rmrs::exports

#endif
