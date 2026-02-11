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
#include "rmrs_resource_export.h"
#include "turbo_over_commit_def.h"

#include <securec.h>
#include <climits>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "libvirt_helper/rmrs_libvirt_helper.h"
#include "os_helper/rmrs_os_helper.h"
#include "rmrs_config.h"
#include "rmrs_json_helper.h"
#include "rmrs_memory_info.h"
#include "turbo_def.h"
#include "turbo_logger.h"

namespace rmrs::exports {
using namespace rmrs;
using namespace turbo::log;
std::string ResourceExport::hostName{};
std::string ResourceExport::nodeId{};
std::unordered_map<uint16_t, uint16_t> ResourceExport::cpuSocketMap{};
std::unordered_map<uint16_t, uint16_t> ResourceExport::cpuNumaMap{};
std::unordered_map<uint16_t, bool> ResourceExport::numaLocalMap{};
std::vector<NumaInfo> ResourceExport::numaInfos{};
std::string ResourceExport::numaNodePathHead = "/sys/devices/system/node/";
std::string ResourceExport::cpuSocketPathPrefix = "/sys/devices/system/cpu/cpu";
std::string ResourceExport::cpuSocketPath = "topology/physical_package_id";

RmrsResult ResourceExport::remove_elements(std::vector<uint16_t> &vecA, std::vector<uint16_t> &vecB)
{
    vecA.erase(std::remove_if(vecA.begin(), vecA.end(),
                              [&vecB](int elem) { return std::find(vecB.begin(), vecB.end(), elem) != vecB.end(); }),
               vecA.end());
    return RMRS_OK;
}

std::string ResourceExport::GetHostName()
{
    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    } else {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] Failed to Get hostname, hostName is void.";
        return "";
    }
}

RmrsResult ResourceExport::Init()
{
    hostName = GetHostName();
    // 初始化cpuId-socketId对照表
    auto ret = OsHelper::GetSocketCpuRelation(cpuSocketMap);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] Failed to get cpu-socket relation.";
        return RMRS_ERROR;
    }

    // 初始化cpuId-numaId对照表
    ret = OsHelper::GetNumaCPUInfos(cpuSocketMap, cpuNumaMap, numaLocalMap, numaInfos);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] Failed to get cpu-numa relation.";
        return RMRS_ERROR;
    }
#ifndef UB_ENVIRONMENT
    ret = OsHelper::GetRemoteAvailableFlag(numaInfos);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] Get Remote Available Flag failed.";
        return RMRS_ERROR;
    }
#endif

    return RMRS_OK;
}

RmrsResult ResourceExport::CollectVmInfo()
{
    LibvirtHelper libvirtHelper;
    if (libvirtHelper.Connect() != RMRS_OK) {
        return RMRS_ERROR;
    }
    RmrsResult ret = libvirtHelper.GetVmBasicInfo(this);
    libvirtHelper.CloseConn();
    if (ret != RMRS_OK) {
        return RMRS_ERROR;
    }
    OsHelper::GetPidFromUUID(this);
    OsHelper::GetPidCreatTime(this);
    OsHelper::GetVMUsedMemory(this);
    OsHelper::GetVmsPidOnNuma(this);
    OsHelper::UpdateNumaMemInfo(this);
    return RMRS_OK;
}

RmrsResult ResourceExport::CollectNumaInfo()
{
    for (auto &info : numaInfos) {
        info.numaMetaInfo.nodeId = nodeId;
        info.numaMetaInfo.hostName = hostName;
        auto numaId = info.numaMetaInfo.numaId;
        std::vector<pid_t> vmsOnNumaVec(numaVmPidMap[numaId].begin(), numaVmPidMap[numaId].end());
        info.numaVmInfo.vmsOnNuma = vmsOnNumaVec;
        info.numaVmInfo.vmTotalAllocatedCpuNum = vmNumaCpuInfos[numaId];
        info.numaMetaInfo.memFree = numaMemFreeMap[numaId];
        info.numaMetaInfo.memTotal = numaMemTotalMap[numaId];
        info.numaMetaInfo.hugePageTotal = numaHugePageTotalMap[numaId];
        info.numaMetaInfo.hugePageFree = numaHugePageFreeMap[numaId];
        // 注意：可能存在vm跨numa绑定cpu情况，暂定vm不存在跨numa绑定cpu的情况
        info.numaVmInfo.vmTotalMaxMem = vmNumaMaxMemInfos[numaId];
        info.numaVmInfo.vmTotalUsedMem = vmNumaUsedMemInfos[numaId];
        info.numaVmInfo.freeCpuList = info.numaMetaInfo.cpuIds;
        info.numaVmInfo.vmCpuList = vmNumaCpuIds[numaId];
        RmrsResult ret =
            remove_elements(info.numaVmInfo.freeCpuList, vmNumaCpuIds[numaId]); // 删除freeCpuList中被虚机绑定的cpu
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsResourceExport] Remove vm cpu from freeCpuList failed.";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

RmrsResult ResourceExport::UpdateVmDomainInfo()
{
    for (auto &vmDomainInfo : VmDomainInfos) {
        vmDomainInfo.metaData.pid = uuidPIDMap[vmDomainInfo.metaData.uuid];
        vmDomainInfo.localNumaId = localNumaId[vmDomainInfo.metaData.uuid];
        vmDomainInfo.localUsedMem = localUsedMem[vmDomainInfo.metaData.uuid];
        vmDomainInfo.remoteNumaId = remoteNumaId[vmDomainInfo.metaData.uuid];
        vmDomainInfo.remoteUsedMem = remoteUsedMem[vmDomainInfo.metaData.uuid];
        vmDomainInfo.metaData.pageSize = uuidPagesizeMap[vmDomainInfo.metaData.uuid];
        vmDomainInfo.metaData.vmCreateTime = pidCreatTimeMap[vmDomainInfo.metaData.pid];
    }
    return RMRS_OK;
}

RmrsResult ResourceExport::GetVmInfoImmediately(std::vector<VmDomainInfo> &vmDomainInfos)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsResourceExport] [Immediately] Collect VmInfo started.";
    RmrsResult ret = ReInit();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsResourceExport] [Immediately] ReInit failed.";
        return RMRS_ERROR;
    }
    auto vmResourceCollect = ResourceExport();
    ret = vmResourceCollect.CollectVmInfo();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [Immediately] Collect VmInfo failed.";
        return RMRS_ERROR;
    }
    vmResourceCollect.CollectNumaInfo();
    vmResourceCollect.UpdateVmDomainInfo();
    vmDomainInfos = *vmResourceCollect.GetVmDomainInfos();
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsResourceExport] [Immediately] Collect VmInfo ended.";
    for (const VmDomainInfo info : vmDomainInfos) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [Immediately] Collect VmInfo: " << info.toString() << ".";
    }
    return RMRS_OK;
}

RmrsResult ResourceExport::GetNumaInfoImmediately(std::vector<NumaInfo> &numaInfos)
{
    RmrsResult ret = ReInit();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsResourceExport] [Immediately] ReInit failed.";
        return RMRS_ERROR;
    }
    auto vmResourceCollect = ResourceExport();
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsResourceExport] [Immediately] Collect NumaInfo started.";
    ret = vmResourceCollect.CollectVmInfo();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [Immediately] Collect NumaInfo failed.";
        return RMRS_ERROR;
    }
    vmResourceCollect.CollectNumaInfo();
    vmResourceCollect.UpdateVmDomainInfo();
    numaInfos = *vmResourceCollect.GetNumaInfos();
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsResourceExport] [Immediately] Collect NumaInfo ended.";
    for (const NumaInfo info : numaInfos) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [Immediately] Collect NumaInfo: " << info.toString() << ".";
    }
    return RMRS_OK;
}

static std::mutex g_reInitMutex;

RmrsResult ResourceExport::ReInit()
{
    std::lock_guard<std::mutex> locker(g_reInitMutex);
    numaInfos.clear();
    auto ret = OsHelper::GetSocketCpuRelation(cpuSocketMap);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] Failed to renew cpu-socket relation.";
        return RMRS_ERROR;
    }

    ret = OsHelper::GetNumaCPUInfos(cpuSocketMap, cpuNumaMap, numaLocalMap, numaInfos);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] Failed to renew cpu-numa relation.";
        return RMRS_ERROR;
    }
#ifndef UB_ENVIRONMENT
    ret = OsHelper::GetRemoteAvailableFlag(numaInfos);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] Failed to renew remote available flag.";
        return RMRS_ERROR;
    }
#endif
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsResourceExport] ReInit ResourceExport success.";
    return RMRS_OK;
}

bool IsPathValid(const std::filesystem::path &path)
{
    if (!std::filesystem::is_regular_file(path)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ResourceExport][CollectMeminfobyNumaId] Path is not regular file.";
        return false;
    }
    return true;
}

uint32_t ResourceExport::CollectMeminfobyNumaId(int numaid, std::map<std::string, std::string> &meminfo)
{
    // 不绑定numa就取节点级的
    std::string pathStr = numaid >= 0 ? (numaNodePathHead + "node" + std::to_string(numaid) + "/meminfo") :
                                        "/proc/meminfo";
    std::filesystem::path path;
    try {
        path = std::filesystem::canonical(pathStr);
    } catch (const std::filesystem::filesystem_error &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ResourceExport][CollectMeminfobyNumaId] Path resolution failed= " << e.what() << ".";
        return RMRS_ERROR;
    }

    if (!IsPathValid(path)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ResourceExport][CollectMeminfobyNumaId] Path is invalid.";
        return RMRS_ERROR;
    }

    if (!HandleMemInfoFile(numaid, path, meminfo)) {
        return RMRS_ERROR;
    }

    if (numaid < 0) {
        meminfo["SocketId"] = std::to_string(-1);
    } else {
        meminfo["SocketId"] = std::to_string(GetNumaIdToSocketId(numaid));
    }

    return RMRS_OK;
}

bool ResourceExport::HandleMemInfoFile(const int numaid, const std::filesystem::path path,
                                       std::map<std::string, std::string> &meminfo)
{
    std::ifstream file(path);
    file.exceptions(std::ios::badbit);
    if (!file.is_open()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ResourceExport][CollectMeminfobyNumaId] Can not open file.";
        return false;
    }

    std::string line;
    try {
        while (std::getline(file, line)) {
            // 行格式: Node 0 KeyName: value kB
            std::istringstream iss(line);
            std::string nodeStr;
            std::string nodeIdStr;
            std::string key;
            std::string value;
            if (numaid >= 0 && !(iss >> nodeStr >> nodeIdStr >> key >> value)) {
                continue;
            }

            if (numaid < 0 && !(iss >> key >> value)) {
                continue;
            }

            // 去掉 key 末尾的冒号
            if (!key.empty() && key.back() == ':') {
                key.pop_back();
            }

            meminfo[key] = value;
        }
    } catch (const std::ios_base::failure &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[CollectMeminfobyNumaId] Getline failed, exception=" << e.what() << ".";
        meminfo.clear();
        return false;
    }

    return true;
}


// 获取系统中存在的NUMA本地节点编号
std::set<uint16_t> ResourceExport::GetLocalNodeIds()
{
    std::set<uint16_t> localNodeIds;

    for (const auto &entry : std::filesystem::directory_iterator(numaNodePathHead)) {
        if (!entry.is_directory())
            continue;

        uint16_t localNodeId = 0;
        if (IsValidLocalNumaNode(entry, localNodeId)) {
            localNodeIds.insert(localNodeId);
        }
    }

    return localNodeIds;
}

bool ResourceExport::IsValidLocalNumaNode(const std::filesystem::directory_entry &entry, uint16_t &localNodeId)
{
    std::smatch match;
    std::string path = entry.path().string();
    if (!std::regex_search(path, match, std::regex("node(\\d+)"))) {
        return false;
    }

    try {
        localNodeId = static_cast<uint16_t>(std::stoi(match[1]));
    } catch (const std::exception &e) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[GetLocalNodeIds] Failed to parse node, path=" << path << ", error: " << e.what();
        return false;
    }
    std::ifstream cpulist(entry.path() / "cpulist");
    if (!cpulist.is_open()) {
        return false;
    }
    std::string content;
    cpulist >> content;
    return !content.empty();
}

// 填充pidInfo，并根据localNumaId所占用内存大小对localNumaIds进行降序排序
mempooling::PidInfo ResourceExport::BuildPidInfoFromNodePages(pid_t pid, bool hasLibvirt,
                                                              const std::unordered_map<uint16_t, uint64_t> &nodePages,
                                                              const std::set<uint16_t> &localNodeIds)
{
    mempooling::PidInfo pidInfo{.pid = pid};
    std::vector<std::pair<uint16_t, uint64_t>> localVec;

    for (const auto &[nodeId, pages] : nodePages) {
        mempooling::MetaNumaInfo metaNumaInfo;
        uint64_t memBytes;
        if (hasLibvirt) {
            memBytes = (pages * mempooling::over_commit::VIRT_SIZE) << mempooling::over_commit::KB2BYTE;
        } else {
            memBytes = (pages * mempooling::over_commit::CTR_SIZE) << mempooling::over_commit::KB2BYTE;
        }
        metaNumaInfo.numaId = nodeId;
        metaNumaInfo.numaUsedMem = memBytes;

        if (localNodeIds.count(nodeId)) {
            pidInfo.totalLocalUsedMem += memBytes;
            metaNumaInfo.isLocalNuma = true;
            int socketId = GetNumaIdToSocketId(nodeId);
            if (socketId >= 0) {
                metaNumaInfo.socketId = socketId;
            } else {
                UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ContainerPidNumaInfo] Get socketId failed, numaId=" << nodeId << ".";
            }
            localVec.emplace_back(nodeId, memBytes);
        } else {
            pidInfo.totalRemoteUsedMem += memBytes;
            metaNumaInfo.isLocalNuma = false;
        }
        pidInfo.metaNumaInfos.emplace_back(metaNumaInfo);
    }

    std::sort(localVec.begin(), localVec.end(), [](const auto &a, const auto &b) { return a.second > b.second; });

    for (const auto &[nodeId, memBytes] : localVec) {
        pidInfo.localNumaIds.push_back(nodeId);
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] Collect pid=" << pid << ", local NumaId=" << nodeId << ", used_Mem=" << memBytes
            << "Byte.";
    }
    return pidInfo;
}

// 从/proc/[pid]/numa_maps提取容器进程在各numa节点内存并分类为本地/远端
RmrsResult ResourceExport::CollectPidNumaInfo(const std::vector<pid_t> &pids,
                                              std::vector<mempooling::PidInfo> &pidInfos)
{
    std::set<uint16_t> localNodeIds = GetLocalNodeIds();
    for (auto &pid : pids) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] Start to collect pid=" << pid << ".";
        std::string fileContent;
        auto vPidStr = std::to_string(pid);
        if (rmrs::OsHelper::ReadNumaMap(vPidStr, fileContent) != RMRS_OK || fileContent.empty()) {
            continue;
        }

        std::istringstream iss(fileContent);

        if (!HandlePidNumaInfoFile(iss, pid, localNodeIds, pidInfos)) {
            return RMRS_ERROR;
        }
    }

    if (pidInfos.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] PidInfos is empty. Collect failed.";
        return RMRS_ERROR;
    }

    return RMRS_OK;
}

bool ResourceExport::HandlePidNumaInfoFile(std::istringstream &iss, const pid_t pid,
                                           const std::set<uint16_t> &localNodeIds,
                                           std::vector<mempooling::PidInfo> &pidInfos)
{
    std::unordered_map<uint16_t, uint64_t> nodePages;
    std::string line;
    std::regex nodePattern("N(\\d+)=([0-9]+)");
    std::vector<std::string> allLines;
    bool hasLibvirt = false;
    while (std::getline(iss, line)) {
        if (line.find("libvirt") != std::string::npos) {
            hasLibvirt = true;
        }
        allLines.push_back(line);
    }

    try {
        for (const auto &l : allLines) {
            if (hasLibvirt && l.find("libvirt") == std::string::npos) {
                continue;
            }

            if (!hasLibvirt && l.find("anon") == std::string::npos) {
                continue;
            }

            for (std::sregex_iterator it(l.begin(), l.end(), nodePattern); it != std::sregex_iterator(); ++it) {
                    uint16_t nodeIdHandle = static_cast<uint16_t>(std::stoi((*it)[1]));
                    uint64_t pages = std::stoull((*it)[2]);
                    nodePages[nodeIdHandle] += pages;
            }
        }
    } catch (const std::exception &e) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] Failed to parse file, error=" << e.what();
        pidInfos.clear();
        return false;
    }

    if (!nodePages.empty()) {
        pidInfos.push_back(BuildPidInfoFromNodePages(pid, hasLibvirt, nodePages, localNodeIds));
    }

    return true;
}

// 读取文件内容（只取第一行）
std::string ReadFileFirstLine(const std::string &pathStr)
{
    std::filesystem::path path;
    try {
        path = std::filesystem::canonical(pathStr);
    } catch (const std::filesystem::filesystem_error &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ResourceExport][ReadFileFirstLine] Path resolution failed= " << e.what() << ".";
        return "";
    }
    if (!IsPathValid(path)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ResourceExport][ReadFileFirstLine] Path is invalid.";
        return "";
    }
    std::ifstream file(path);
    file.exceptions(std::ios::badbit);
    if (!file.is_open()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ResourceExport][ReadFileFirstLine] ReadFileFirstLine fail to open.";
        return "";
    }
    std::string line;
    try {
        std::getline(file, line);
        return line;
    } catch (const std::ios_base::failure &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ResourceExport][ReadFileFirstLine] ReadFileFirstLine failed, exception=" << e.what() << ".";
        return "";
    }
}

// 解析 cpulist 字符串为一组 CPU 编号（支持 0,2,4-6 等格式）
void AddCpuToList(std::vector<int> &list, int start, int end)
{
    for (int i = start; i <= end; ++i) {
        list.push_back(i);
    }
}
std::vector<int> ParseCpuList(const std::string &cpuList)
{
    std::vector<int> cpus;
    std::istringstream iss(cpuList);
    std::string token;
    iss.exceptions(std::ios::badbit);
    try {
        while (std::getline(iss, token, ',')) {
            size_t dash = token.find('-');
            if (dash != std::string::npos) {
                int start = std::stoi(token.substr(0, dash));
                int end = std::stoi(token.substr(dash + 1));
                AddCpuToList(cpus, start, end);
            } else {
                cpus.push_back(std::stoi(token));
            }
        }
    } catch (const std::ios_base::failure &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] Exception during parse CpuList: " << e.what() << ".";
        return {};
    } catch (const std::invalid_argument &e) {
        throw std::invalid_argument(std::string("Invalid argument: ") + e.what());
    } catch (const std::out_of_range &e) {
        throw std::out_of_range(std::string("Out of range: ") + e.what());
    }

    std::stringstream cpusStream;
    for (int cpu : cpus) {
        cpusStream << cpu << " ";
    }
    std::string cpusStr = cpusStream.str();
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ContainerPidNumaInfo] ParseCpuList success, CpuList = "
                                                       << "[" << cpusStr << "]";
    return cpus;
}

int SafeStoi(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        return std::stoi(str);
    } catch (const std::invalid_argument &e) {
        throw std::invalid_argument("invalid argument");
    } catch (const std::out_of_range &e) {
        throw std::out_of_range("out of range");
    }
}

int ResourceExport::GetSocketIdFromCpu(int cpuId)
{
    std::filesystem::path path = std::filesystem::path((cpuSocketPathPrefix + std::to_string(cpuId))) / cpuSocketPath;
    try {
        path = std::filesystem::canonical(path);
    } catch (const std::filesystem::filesystem_error &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ResourceExport][GetSocketIdFromCpu] Path resolution failed= " << e.what() << ".";
        return RMRS_ERROR_SIGN_INT;
    }
    if (!IsPathValid(path)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ContainerPidNumaInfo] Path is invailded.";
        return RMRS_ERROR_SIGN_INT;
    }
    std::string socketStr = ReadFileFirstLine(path.string());
    if (socketStr.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] GetSocketIdFromCpu failed, socketStr is empty.";
        return RMRS_ERROR_SIGN_INT;
    }
    try {
        return SafeStoi(socketStr);
    } catch (const std::exception &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] GetSocketIdFromCpu failed, " << e.what() << ".";
        return RMRS_ERROR_SIGN_INT;
    }
}

int ResourceExport::GetNumaIdToSocketId(uint16_t numaId)
{
    std::string numaIdStr = "node" + std::to_string(numaId);
    std::filesystem::path path = std::filesystem::path(numaNodePathHead) / numaIdStr / "cpulist";
    std::string cpuListStr = ReadFileFirstLine(path.string());
    if (cpuListStr.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] Get cpulist failed, numaId = " << numaId;
        return RMRS_ERROR_SIGN_INT;
    }
    vector<int> cpus;
    try {
        cpus = ParseCpuList(cpuListStr);
    } catch (const std::exception &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] Parse cpulist failed, caught in ParseCpuList=" << e.what();
        return RMRS_ERROR_SIGN_INT;
    }

    if (cpus.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ContainerPidNumaInfo] Parse cpulist failed, cpuListStr = " << cpuListStr;
        return RMRS_ERROR_SIGN_INT;
    }

    for (int cpu : cpus) {
        int socket = GetSocketIdFromCpu(cpu);
        if (socket >= 0)
            return socket;
    }

    UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ContainerPidNumaInfo] Get socketId failed, numaId = " << numaId;
    return RMRS_ERROR_SIGN_INT;
}

std::vector<VmDomainInfo> *ResourceExport::GetVmDomainInfos()
{
    return &this->VmDomainInfos;
}

std::unordered_map<uint16_t, uint16_t> *ResourceExport::GetVmNumaCpuInfos()
{
    return &this->vmNumaCpuInfos;
}

std::unordered_map<uint16_t, std::vector<uint16_t>> *ResourceExport::GetVmNumaCpuIds()
{
    return &this->vmNumaCpuIds;
}

std::unordered_map<uint16_t, uint64_t> *ResourceExport::GetVmNumaMaxMemInfos()
{
    return &this->vmNumaMaxMemInfos;
}

std::unordered_map<uint16_t, uint64_t> *ResourceExport::GetVmNumaUsedMemInfos()
{
    return &this->vmNumaUsedMemInfos;
}

std::vector<NumaInfo> *ResourceExport::GetNumaInfos()
{
    return &numaInfos;
}

std::unordered_map<uint16_t, uint16_t> *ResourceExport::GetCpuSocketMap()
{
    return &cpuSocketMap;
}

std::unordered_map<uint16_t, uint16_t> *ResourceExport::GetCpuNumaMap()
{
    return &cpuNumaMap;
}

std::unordered_map<std::string, int> *ResourceExport::GetUuidPIDMap()
{
    return &this->uuidPIDMap;
}

std::unordered_map<int, std::string> *ResourceExport::GetPidUUIDMap()
{
    return &this->pidUUIDMap;
}

std::unordered_map<std::string, uint16_t> *ResourceExport::GetUuidNumaMap()
{
    return &this->uuidNumaMap;
}

std::unordered_map<int, time_t> *ResourceExport::GetPidCreatTimeMap()
{
    return &this->pidCreatTimeMap;
}

std::vector<pid_t> *ResourceExport::GetVPidList()
{
    return &this->vPidList;
}

std::vector<std::string> *ResourceExport::GetUuidList()
{
    return &this->uuidList;
}

std::unordered_map<std::string, uint64_t> *ResourceExport::GetRemoteUsedMem()
{
    return &this->remoteUsedMem;
}

std::unordered_map<std::string, uint64_t> *ResourceExport::GetLocalUsedMem()
{
    return &this->localUsedMem;
}

std::unordered_map<std::string, uint16_t> *ResourceExport::GetRemoteNumaId()
{
    return &this->remoteNumaId;
}

std::unordered_map<std::string, uint16_t> *ResourceExport::GetLocalNumaId()
{
    return &this->localNumaId;
}

std::unordered_map<uint16_t, bool> *ResourceExport::GetNumaLocalMap()
{
    return &numaLocalMap;
}

std::unordered_map<uint16_t, uint64_t> *ResourceExport::GetNumaMemTotalMap()
{
    return &this->numaMemTotalMap;
}

std::unordered_map<uint16_t, uint64_t> *ResourceExport::GetNumaMemFreeMap()
{
    return &this->numaMemFreeMap;
}

std::unordered_map<uint16_t, uint64_t> *ResourceExport::GetHugePageTotalMap()
{
    return &this->numaHugePageTotalMap;
}

std::unordered_map<uint16_t, uint64_t> *ResourceExport::GetHugePageFreeMap()
{
    return &this->numaHugePageFreeMap;
}

std::unordered_map<std::string, std::string> *ResourceExport::GetUuidNameMap()
{
    return &this->uuidNameMap;
}

std::unordered_map<std::string, std::uint64_t> *ResourceExport::GetUuidPagesizeMap()
{
    return &this->uuidPagesizeMap;
}

std::unordered_map<uint16_t, std::unordered_set<pid_t>> *ResourceExport::GetNumaVmPidMap()
{
    return &this->numaVmPidMap;
}

} // namespace rmrs::exports