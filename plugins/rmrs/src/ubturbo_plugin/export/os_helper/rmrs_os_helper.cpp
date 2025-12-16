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
#include "rmrs_os_helper.h"

#include <dirent.h>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <array>

#include "rmrs_config.h"
#include "rmrs_file_util.h"
#include "rmrs_string_util.h"
#include "turbo_logger.h"
#include "securec.h"

namespace rmrs {

using namespace turbo::log;
using std::ifstream;
using std::lock_guard;
using std::nothrow;
// using std::regex;
using std::stringstream;
string OsHelper::cpuSocketPathPrefix = "/sys/devices/system/cpu";
string OsHelper::cpuSocketPath = "/topology/physical_package_id";
string OsHelper::numaPathPrefix = "/sys/devices/system/node";
string OsHelper::procPathPrefix = "/proc";
string OsHelper::qemuPathPrefix = "/var/run/libvirt/qemu";
const string FREE_HUGE_PAGES_PATH = "/hugepages/hugepages-2048kB/free_hugepages";
const string TOTAL_HUGE_PAGES_PATH = "/hugepages/hugepages-2048kB/nr_hugepages";
const string MEM_TOTAL = "MemTotal";
const string MEM_FREE = "MemFree";
const uint16_t USUAL_INFO_SIZE = 1;
const int MEM_INFO_SIZE = 2;
const int VM_NUMA_NUM_MAX = 2;
const int VM_PAGESIZE_INDEX = 1;
const int MEM_INFO_NAME_LOC = 1;
const int MEM_INFO_VALUE_LOC = 2;
const int MATCH_NUMA_ID = 1;
const int MATCH_NUMA_VALUE = 2;
const int LOCAL_NUMA_ID_INDEX = 1;
const int LOCAL_NUMA_MEM_INDEX = 2;
const int REMOTE_NUMA_ID_INDEX = 3;
const int REMOTE_NUMA_MEM_INDEX = 4;
const std::string OBMM_CFG_FILE = "/root/obmm/obmm_configure.py";
const std::string CMDLINE_FILE = "/proc/cmdline";
const int MAX_LOCAL_NUMA_NODES = 4;
const int HEX_BASE = 16;
const int STANDARD_PAGESIZE = 4;
constexpr uint64_t HUGE_PAGE_NUM_TO_KB = 2 * 1024;
constexpr int CMD_BUFFER_SIZE = 256;
constexpr const char* CAT_SCRIPT_CAT_PATH = "sudo /usr/local/bin/cat.sh";
constexpr const char* CAT_SCRIPT_TAIL = "2>&1";

/**
 * 读取/proc目录下指定PID的进程信息, 获取启动时间
 * @param pid
 * @param timestamp
 * @return
 */
uint32_t OsHelper::GetProcessStartTime(pid_t pid, time_t &timestamp)
{
    // 构建目标进程的stat文件路径
    stringstream ssPath;
    ssPath << "/proc/" << pid << "/stat";

    // 打开指定PID的stat文件
    auto lineInfo = vector<string>();
    auto ret = RmrsFileUtil::GetFileInfo(ssPath.str(), lineInfo);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Open the stat file of pid = " << pid << " failed.";
        return ret;
    }
    string line = lineInfo[0];
    std::istringstream iss(line);
    string token;
    for (size_t i = 0; i < startTimeIndexInProc; ++i) {
        iss >> token;
    }
    // 读取启动时间（单位为jiffies）
    unsigned long startTimeInJiffies;
    iss >> startTimeInJiffies;

    // 转换jiffies到秒
    clock_t ticksPerSecond = sysconf(_SC_CLK_TCK);
    if (ticksPerSecond == -1 || ticksPerSecond == 0) {
        return -1;
    }

    // 获取系统启动时间
    std::ifstream uptimeFile("/proc/uptime");
    double uptimeInSec;
    if (!(uptimeFile >> uptimeInSec)) {
        return -1;
    }
    time_t currentTime = std::time(nullptr);
    time_t sysStartTime = currentTime - static_cast<time_t>(uptimeInSec);

    // 计算进程启动时间
    timestamp = sysStartTime + static_cast<time_t>(static_cast<long long>(startTimeInJiffies) /
                                                   std::abs(static_cast<long long>(ticksPerSecond)));

    // 格式化日期和时间
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&timestamp), "%Y-%m-%d %H:%M:%S");
    std::string dtime = oss.str();

    return RMRS_OK;
}

/**
 * 读取/sys/devices/system/cpu下文件, 获取cpu core -- socket的映射关系
 * @param cpuSocketMap
 * @return
 */
uint32_t OsHelper::GetSocketCpuRelation(unordered_map<uint16_t, uint16_t> &cpuSocketMap)
{
    auto dir = opendir(cpuSocketPathPrefix.c_str());
    if (dir == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[OsHelper] Get cpu info failed from file system.";
        return RMRS_ERROR;
    }
    // 读CPU的文件目录，获取CPU到socket的映射关系
    dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        auto folderName = std::string(entry->d_name);
        if (folderName.find("cpu") != std::string::npos) {
            string pattern = "^cpu\\d+$";
            if (RmrsFileUtil::IsSpecifiedPath(folderName, pattern) != RMRS_OK) {
                continue;
            }
            auto index = folderName.find("cpu");
            auto cpuId = RmrsStringUtil::SafeStou16(folderName.substr(index + string("cpu").size()));
            stringstream socketPath;
            socketPath << cpuSocketPathPrefix << "/" << folderName << cpuSocketPath;
            vector<string> socketInfo;
            auto res = RmrsFileUtil::GetFileInfo(socketPath.str(), socketInfo);
            if (res != RMRS_OK || socketInfo.size() != 1 || socketInfo[0].empty()) {
                UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[RmrsResourceExport] [OsHelper] Socket_info_size = " << socketInfo.size() << ".";
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[RmrsResourceExport] [OsHelper] Get socket info failed, socket info size is invalid.";
                closedir(dir);
                return res;
            }
            auto socketId = RmrsStringUtil::SafeStou16(socketInfo[0]);
            cpuSocketMap.emplace(cpuId, socketId);
        }
    }
    closedir(dir);
    return RMRS_OK;
}

/**
 * 读取/sys/devices/system/node/目录下文件, 获取Numa信息, cpu-numa映射关系.
 * @param cpuSocketMap
 * @param cpuNumaMap
 * @param numaCpuInfos
 * @return
 */
uint32_t OsHelper::GetNumaCPUInfos(unordered_map<uint16_t, uint16_t> &cpuSocketMap,
                                   unordered_map<uint16_t, uint16_t> &cpuNumaMap,
                                   unordered_map<uint16_t, bool> &numaLocalMap, vector<NumaInfo> &numaInfos)
{
    auto dir = opendir(numaPathPrefix.c_str());
    if (dir == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[OsHelper] Get numa num info fail from file system.";
        return RMRS_ERROR;
    }
    // 用于现在不知道实际拓扑，写死后在不同机器上需要更改源代码的情况，先给了一个逻辑编号
    int logicNumaIndex = 0;
    int logicSocketIndex = 0;

    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (RmrsFileUtil::IsSpecifiedPath(entry->d_name, "^node\\d+$") == RMRS_OK) {
            string nodePath = "";
            nodePath.append(numaPathPrefix).append(&std::filesystem::path::preferred_separator, 1)
                .append(entry->d_name);
            string cpuPath = "";
            cpuPath.append(nodePath).append(&std::filesystem::path::preferred_separator, 1).append("cpulist");
            auto nodeInfo = std::vector<std::string>();
            if (getFileContent(cpuPath, nodeInfo, "cpulist") != RMRS_OK) {
                closedir(dir);
                return RMRS_ERROR;
            }
            if (checkFileContent(nodeInfo, "cpulist", USUAL_INFO_SIZE) != RMRS_OK) {continue;}
            NumaInfo tempNumaInfo{};
            auto numaId = ReadCpuInfo(nodeInfo, tempNumaInfo, cpuSocketMap, entry->d_name);
            tempNumaInfo.numaMetaInfo.logicId = logicNumaIndex++;
            if (logicNumaIndex >= numaIndex) {
                logicSocketIndex++;
            }
            tempNumaInfo.numaMetaInfo.logicSocketId = logicSocketIndex;
            for (unsigned short &i : tempNumaInfo.numaMetaInfo.cpuIds) {
                cpuNumaMap.emplace(i, numaId); // cpu与Numa映射关系
            }
            numaLocalMap[numaId] = tempNumaInfo.numaMetaInfo.isLocal;
            auto res = getMemInfo(nodePath, tempNumaInfo);
            if (res == RMRS_ERROR_NOENT) {
                closedir(dir);
                return RMRS_ERROR;
            } else if (res == RMRS_ERROR) {
                continue;
            }

            numaInfos.push_back(tempNumaInfo); // 主机上numa-cpu信息
        }
    }
    closedir(dir);
    return RMRS_OK;
}

uint32_t OsHelper::UpdateNumaMemInfo(ResourceExport *resourceCollect)
{
    auto dir = opendir(numaPathPrefix.c_str());
    if (dir == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Get numa nums info failed from file system.";
        return RMRS_ERROR;
    }
    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (RmrsFileUtil::IsSpecifiedPath(entry->d_name, "^node\\d+$") == RMRS_OK) {
            string nodePath = "";
            nodePath.append(numaPathPrefix)
                .append(&std::filesystem::path::preferred_separator, 1)
                .append(entry->d_name);
            auto res = updateMemInfo(nodePath, resourceCollect, entry->d_name);
            if (res == RMRS_ERROR_NOENT) {
                closedir(dir);
                return RMRS_ERROR;
            } else if (res == RMRS_ERROR) {
                continue;
            }
        }
    }
    closedir(dir);
    return RMRS_OK;
}

RmrsResult OsHelper::getMemInfo(const std::string &nodePath, NumaInfo &tempNumaInfo)
{
    std::string hugePageTotalPath = nodePath + TOTAL_HUGE_PAGES_PATH;
    auto hugePageTotalInfo = std::vector<std::string>();
    if (getFileContent(hugePageTotalPath, hugePageTotalInfo, "hugePageTotal") != RMRS_OK) {
        return RMRS_ERROR_NOENT;
    }
    if (checkFileContent(hugePageTotalInfo, "hugePageTotal", USUAL_INFO_SIZE) != RMRS_OK) {
        return RMRS_ERROR;
    }
    tempNumaInfo.numaMetaInfo.hugePageTotal = RmrsStringUtil::SafeStoull(hugePageTotalInfo[0]);

    std::string hugePageFreePath = nodePath + FREE_HUGE_PAGES_PATH;
    auto hugePageFreeInfo = std::vector<std::string>();
    if (getFileContent(hugePageFreePath, hugePageFreeInfo, "hugePageFree") != RMRS_OK) {
        return RMRS_ERROR_NOENT;
    }
    if (checkFileContent(hugePageFreeInfo, "hugePageFree", USUAL_INFO_SIZE) != RMRS_OK) {
        return RMRS_ERROR;
    }
    tempNumaInfo.numaMetaInfo.hugePageFree = RmrsStringUtil::SafeStoull(hugePageFreeInfo[0]);

    unordered_map<std::string, uint64_t> memInfoMap;
    std::string memInfoPath = "";
    memInfoPath.append(nodePath).append(&std::filesystem::path::preferred_separator, 1).append("meminfo");
    auto memInfo = std::vector<std::string>();
    if (getFileContent(memInfoPath, memInfo, "memInfo") != RMRS_OK) {
        return RMRS_ERROR_NOENT;
    }
    readMemInfo(memInfo, memInfoMap);
    tempNumaInfo.numaMetaInfo.memTotal = memInfoMap[MEM_TOTAL];
    tempNumaInfo.numaMetaInfo.memFree = memInfoMap[MEM_FREE];
    return RMRS_OK;
}

RmrsResult OsHelper::updateMemInfo(const std::string &nodePath, ResourceExport *resourceCollect, std::string folderName)
{
    auto memFreeMap = resourceCollect->GetNumaMemFreeMap();
    auto memTotalMap = resourceCollect->GetNumaMemTotalMap();
    auto hugePageTotalMap = resourceCollect->GetHugePageTotalMap();
    auto hugePageFreeMap = resourceCollect->GetHugePageFreeMap();
    auto index = folderName.find("node");
    auto numaId = RmrsStringUtil::SafeStou16(folderName.substr(index + std::string("node").size()));
    std::string hugePageTotalPath = nodePath + TOTAL_HUGE_PAGES_PATH;
    auto hugePageTotalInfo = std::vector<std::string>();
    if (getFileContent(hugePageTotalPath, hugePageTotalInfo, "hugePageTotal") != RMRS_OK) {
        return RMRS_ERROR_NOENT;
    }
    if (checkFileContent(hugePageTotalInfo, "hugePageTotal", USUAL_INFO_SIZE) != RMRS_OK) {
        return RMRS_ERROR;
    }
    (*hugePageTotalMap)[numaId] = RmrsStringUtil::SafeStoull(hugePageTotalInfo[0]);

    std::string hugePageFreePath = nodePath + FREE_HUGE_PAGES_PATH;
    auto hugePageFreeInfo = std::vector<std::string>();
    if (getFileContent(hugePageFreePath, hugePageFreeInfo, "hugePageFree") != RMRS_OK) {
        return RMRS_ERROR_NOENT;
    }
    if (checkFileContent(hugePageFreeInfo, "hugePageFree", USUAL_INFO_SIZE) != RMRS_OK) {
        return RMRS_ERROR;
    }
    (*hugePageFreeMap)[numaId] = RmrsStringUtil::SafeStoull(hugePageFreeInfo[0]);

    unordered_map<std::string, uint64_t> memInfoMap;
    std::string memInfoPath = "";
    memInfoPath.append(nodePath).append(&std::filesystem::path::preferred_separator, 1).append("meminfo");
    auto memInfo = std::vector<std::string>();
    if (getFileContent(memInfoPath, memInfo, "memInfo") != RMRS_OK) {
        return RMRS_ERROR_NOENT;
    }
    readMemInfo(memInfo, memInfoMap);
    (*memTotalMap)[numaId] = memInfoMap[MEM_TOTAL];
    (*memFreeMap)[numaId] = memInfoMap[MEM_FREE];
    return RMRS_OK;
}

RmrsResult OsHelper::readMemInfo(std::vector<std::string> &memInfos, unordered_map<std::string, uint64_t> &memInfoMap)
{
    std::regex rgx("(\\w+)\\s*:\\s*(\\d+)\\s*(kB)?");
    std::smatch match;
    for (auto memInfo : memInfos) {
        if (std::regex_search(memInfo, match, rgx)) {
            if (match[MEM_INFO_NAME_LOC] == MEM_TOTAL || match[MEM_INFO_NAME_LOC] == MEM_FREE)
                ;
            memInfoMap[match[MEM_INFO_NAME_LOC]] = RmrsStringUtil::SafeStoull(match[MEM_INFO_VALUE_LOC]);
        }
    }
    return RMRS_OK;
}

RmrsResult OsHelper::getFileContent(const std::string &filePath, std::vector<std::string> &info,
                                    const std::string &fileName)
{
    auto ret = RmrsFileUtil::GetFileInfo(filePath, info);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Open the node " << fileName << " file failed.";
        return ret;
    }
    return RMRS_OK;
}

RmrsResult OsHelper::checkFileContent(std::vector<std::string> &info, const std::string &fileName, uint16_t vectorNum)
{
    if (info.size() != vectorNum) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] The node " << fileName
            << " file is unavailable, line num is: " << info.size() << ".";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

int OsHelper::ReadCpuInfo(std::vector<std::string> &nodeInfo, NumaInfo &tempNumaInfo,
                          unordered_map<uint16_t, uint16_t> &cpuSocketMap, std::string folderName)
{
    if (nodeInfo[0].empty()) {
        auto index = folderName.find("node");
        auto numaId = RmrsStringUtil::SafeStou16(folderName.substr(index + std::string("node").size()));
        tempNumaInfo.numaMetaInfo.numaId = numaId;
        tempNumaInfo.numaMetaInfo.isLocal = false;
        return tempNumaInfo.numaMetaInfo.numaId;
    }
    // 该numa绑定的cpu列表和数量
    tempNumaInfo.numaMetaInfo.cpuIds = ParseCPUList(nodeInfo[0]);
    tempNumaInfo.numaMetaInfo.cpuCounts = tempNumaInfo.numaMetaInfo.cpuIds.size();
    tempNumaInfo.numaVmInfo.freeCpuList = tempNumaInfo.numaMetaInfo.cpuIds;
    tempNumaInfo.numaMetaInfo.isLocal = true;
    auto index = folderName.find("node");
    auto numaId = RmrsStringUtil::SafeStou16(folderName.substr(index + std::string("node").size()));
    tempNumaInfo.numaMetaInfo.socketId = cpuSocketMap[tempNumaInfo.numaMetaInfo.cpuIds[0]];
    tempNumaInfo.numaMetaInfo.numaId = numaId;
    return numaId;
}

/**
 * 读取/proc目录下进程信息, 查询进程已使用内存情况
 * @return
 */
uint32_t OsHelper::GetVMUsedMemory(ResourceExport *resourceCollect)
{
    auto vPidList = resourceCollect->GetVPidList();
    auto pidUUIDMap = resourceCollect->GetPidUUIDMap();
    auto ret = RMRS_OK;
 
    for (auto vPid : *vPidList) {
        auto vPidStr = std::to_string(vPid);
        ret |= GetInfoFromNumaMaps((*pidUUIDMap)[vPid], vPidStr, resourceCollect);
        ret |= GetVmPageSizeFromNumaMaps((*pidUUIDMap)[vPid], vPidStr, resourceCollect);
    }
    return ret;
}

uint32_t OsHelper::GetVmsPidOnNuma(ResourceExport *resourceCollect)
{
    auto numaVmPidMap = resourceCollect->GetNumaVmPidMap();
    auto uuidPIDMap = resourceCollect->GetUuidPIDMap();
    auto localNumaId = resourceCollect->GetLocalNumaId();
    auto localUsedMem = resourceCollect->GetLocalUsedMem();
    auto remoteNumaId = resourceCollect->GetRemoteNumaId();
    auto remoteUsedMem = resourceCollect->GetRemoteUsedMem();
    numaVmPidMap->clear();
    for (const auto &p : *localUsedMem) {
        auto uuid = p.first;
        if (p.second > 0) {
            auto numaId = (*localNumaId)[uuid];
            (*numaVmPidMap)[numaId].insert((*uuidPIDMap)[uuid]);
        }
    }
    for (const auto &p : *remoteUsedMem) {
        auto uuid = p.first;
        if (p.second > 0) {
            auto numaId = (*remoteNumaId)[uuid];
            (*numaVmPidMap)[numaId].insert((*uuidPIDMap)[uuid]);
        }
    }
    return RMRS_OK;
}

vector<uint16_t> OsHelper::ParseCPUList(const string &line)
{
    vector<uint16_t> cpus;
    stringstream ss(line);
    string token;
    while (getline(ss, token, ',')) {
        size_t dashPos = token.find('-');
        if (dashPos != string::npos) {
            auto start = RmrsStringUtil::SafeStou16(token.substr(0, dashPos));
            auto end = RmrsStringUtil::SafeStou16(token.substr(dashPos + 1));
            for (auto i = start; i <= end; ++i) {
                cpus.push_back(i);
            }
        } else {
            cpus.push_back(RmrsStringUtil::SafeStou16(token));
        }
    }
    return cpus;
}

std::string OsHelper::ExecCommand(const std::string &cmd)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsResourceExport] [OsHelper] ExecCommand start.";
    std::array<char, CMD_BUFFER_SIZE> buffer;
    std::string result;
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsResourceExport] [OsHelper] ExecCommand cmd = " << cmd << ".";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsResourceExport] [OsHelper] Failed to popen!";
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsResourceExport] [OsHelper] ExecCommand end.";
    return result;
}

RmrsResult OsHelper::GetVmPageSizeFromNumaMaps(const std::string &uuid, const std::string &path,
                                               ResourceExport *resourceCollect)
{
    // 调用脚本
    std::string fileContent;
    ReadNumaMap(path, fileContent);
    bool updateFlag = false;
    auto uuidPagesizeMap = resourceCollect->GetUuidPagesizeMap();
    std::istringstream file(fileContent);
    std::string line;
    std::regex keywordRegex("(libvirt.*qemu.*dirty.*N(\\d+)=(\\d+))");
    std::regex pagesizeRegex("kernelpagesize_kB=(\\d+)");
    while (std::getline(file, line)) {
        std::smatch pagesizeMatch;
        if (std::regex_search(line, keywordRegex) && std::regex_search(line, pagesizeMatch, pagesizeRegex)) {
            (*uuidPagesizeMap)[uuid] = RmrsStringUtil::SafeStoull(pagesizeMatch[VM_PAGESIZE_INDEX]);
            updateFlag = true;
            break;
        }
    }

    if (!updateFlag) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Vm: " << uuid << " is not created with hugepages.";
        (*uuidPagesizeMap)[uuid] = STANDARD_PAGESIZE;
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

RmrsResult OsHelper::ReadNumaMap(const std::string &pidStr, std::string &fileContent)
{
    char cmdBuf[CMD_BUFFER_SIZE];
    int ret = snprintf_s(cmdBuf, sizeof(cmdBuf), sizeof(cmdBuf) - 1, "%s %s %s", CAT_SCRIPT_CAT_PATH, pidStr.c_str(),
                         CAT_SCRIPT_TAIL);
    if (ret == -1) {
        cmdBuf[0] = '\0';
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] The snprintf_s operation has failed.";
        return RMRS_ERROR;
    }
    std::string cmd(cmdBuf);
 
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsResourceExport] [OsHelper] Input Param pid=" << pidStr << ".";
 
    // 调用脚本
    try {
        fileContent = ExecCommand(cmd);
    } catch (...) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Cannot execute cat.sh for pid=" << pidStr << ".";
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Failed to read numa_maps file.";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

uint32_t OsHelper::GetInfoFromNumaMaps(const std::string &uuid, const std::string &path,
                                       ResourceExport *resourceCollect)
{
    // 调用脚本
    std::string fileContent;
    ReadNumaMap(path, fileContent);
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsResourceExport] [OsHelper] Size = " << fileContent.size() << ".";

    std::istringstream file(fileContent);
    std::string line;
    std::regex keywordRegex("(libvirt.*qemu.*dirty.*N(\\d+)=(\\d+))");
    std::regex n1Regex("N(\\d+)=(\\d+)");
    std::regex n2Regex("N(\\d+)=(\\d+) N(\\d+)=(\\d+)");
    std::unordered_map<uint16_t, uint64_t> usedMemMap;
    auto numaLocalMap = ResourceExport::GetNumaLocalMap();
    auto uuidNumaMap = resourceCollect->GetUuidNumaMap();
    auto localNumaIdMap = resourceCollect->GetLocalNumaId();
    auto remoteNumaIdMap = resourceCollect->GetRemoteNumaId();
    auto localUsedMemMap = resourceCollect->GetLocalUsedMem();
    auto remoteUsedMemMap = resourceCollect->GetRemoteUsedMem();
    while (std::getline(file, line)) {
        std::smatch n1Match;
        std::smatch n2Match;
        if (std::regex_search(line, keywordRegex) && std::regex_search(line, n2Match, n2Regex)) {
            usedMemMap[RmrsStringUtil::SafeStoi16(n2Match[LOCAL_NUMA_ID_INDEX])] +=
                RmrsStringUtil::SafeStoull(n2Match[LOCAL_NUMA_MEM_INDEX]) * HUGE_PAGE_NUM_TO_KB;
            usedMemMap[RmrsStringUtil::SafeStoi16(n2Match[REMOTE_NUMA_ID_INDEX])] +=
                RmrsStringUtil::SafeStoull(n2Match[REMOTE_NUMA_MEM_INDEX]) * HUGE_PAGE_NUM_TO_KB;
        } else if (std::regex_search(line, keywordRegex) && std::regex_search(line, n1Match, n1Regex)) {
            usedMemMap[RmrsStringUtil::SafeStoi16(n1Match[LOCAL_NUMA_ID_INDEX])] +=
                RmrsStringUtil::SafeStoull(n1Match[LOCAL_NUMA_MEM_INDEX]) * HUGE_PAGE_NUM_TO_KB;
        }
    }
    if (usedMemMap.size() > VM_NUMA_NUM_MAX || usedMemMap.empty() ||
        usedMemMap.find((*uuidNumaMap)[uuid]) == usedMemMap.end()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Used numa is not match, size " << usedMemMap.size() << ".";
        return RMRS_ERROR;
    }
    for (const auto &pair : usedMemMap) {
        if ((*numaLocalMap)[pair.first]) {
            (*localNumaIdMap)[uuid] = pair.first;
            (*localUsedMemMap)[uuid] = pair.second;
        } else {
            (*remoteNumaIdMap)[uuid] = pair.first;
            (*remoteUsedMemMap)[uuid] = pair.second;
        }
    }
    return RMRS_OK;
}

/**
 * 维护uuid-pid和pid-uuid两张表
 * @return
 */
uint32_t OsHelper::GetPidFromUUID(ResourceExport *resourceCollect)
{
    auto uuidPIDMap = resourceCollect->GetUuidPIDMap();
    auto pidUUIDMap = resourceCollect->GetPidUUIDMap();
    auto uuidNameMap = resourceCollect->GetUuidNameMap();
    auto uuidList = resourceCollect->GetUuidList();
    uuidPIDMap->clear();
    for (auto &uuid : *uuidList) {
        int pid = GetPidByVmName((*uuidNameMap)[uuid]);
        if (pid == -1) {
            continue;
        }
        (*uuidPIDMap)[uuid] = pid;
        (*pidUUIDMap)[pid] = uuid;
    }
    return RMRS_OK;
}


/**
 * 读取/var/run/libvirt/qemu的文件，根据虚机name找到同名.pid文件，读取虚机对应的PID
 * @param name const string &: 虚拟机name
 * @return pid uint16_t: 虚拟机进程PID
 */
pid_t OsHelper::GetPidByVmName(const string &name)
{
    pid_t pid = -1;
    DIR *dir = opendir(OsHelper::qemuPathPrefix.c_str());
    if (dir == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Open " << OsHelper::qemuPathPrefix << " failed";
        return pid;
    }
    stringstream ssPath;
    vector<string> infoLines;
    ssPath << OsHelper::qemuPathPrefix << "/" << name << ".pid";
    auto ret = RmrsFileUtil::GetFileInfo(ssPath.str(), infoLines);
    if (ret != RMRS_OK || infoLines.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [OsHelper] Read " << ssPath.str() << " failed.";
        closedir(dir);
        return pid;
    }
    pid = RmrsStringUtil::SafeStopid(infoLines[0]);
    closedir(dir);
    return pid;
}

uint32_t OsHelper::GetPidCreatTime(ResourceExport *resourceCollect)
{
    auto pidCreatTimeMap = resourceCollect->GetPidCreatTimeMap();
    auto vPidList = resourceCollect->GetVPidList();
    auto uuidList = resourceCollect->GetUuidList();
    auto uuidPIDMap = resourceCollect->GetUuidPIDMap();
    pidCreatTimeMap->clear();
    vPidList->clear();
    for (auto &uuid : *uuidList) {
        time_t timestamp;
        auto pid = (*uuidPIDMap)[uuid];
        GetProcessStartTime(pid, timestamp);
        (*pidCreatTimeMap)[pid] = timestamp;
        vPidList->push_back(pid);
    }

    return RMRS_OK;
}

static const int HCOM_NUMA_ID = 63;
RmrsResult OsHelper::GetRemoteAvailableFlag(vector<NumaInfo> &numaInfos)
{
    for (auto &numaInfo : numaInfos) {
        if (numaInfo.numaMetaInfo.numaId == HCOM_NUMA_ID && numaInfo.numaMetaInfo.isLocal == false) {
            numaInfo.numaMetaInfo.isRemoteAvailable = false;
            break;
        }
    }
    return RMRS_OK;
}
} // namespace rmrs