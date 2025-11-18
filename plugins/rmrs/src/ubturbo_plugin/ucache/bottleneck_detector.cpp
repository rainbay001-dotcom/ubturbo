/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
 
#include "bottleneck_detector.h"
 
#include <dirent.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

#include "rmrs_file_util.h"
#include "rmrs_config.h"
#include "rmrs_error.h"
#include "turbo_logger.h"
#include "rmrs_string_util.h"
 
namespace ucache {
namespace bottleneck_detector {
using namespace turbo::log;
using namespace rmrs;

namespace {
const std::string K8S_CONTAINER_PATH = "/sys/fs/cgroup/cpuset/kubepods.slice/kubepods-besteffort.slice/";
constexpr int EXPECTED_MATCH_GROUPS = 2;
constexpr size_t CPU_INDEX_START = 3;
constexpr size_t MIN_CPU_STR_LENGTH = 4;

constexpr double BOTTLENECK_THRESHOLD_IOWAIT = 0.1;
constexpr uint64_t BOTTLENECK_THRESHOLD_LEVEL2 = 10;
constexpr uint64_t BOTTLENECK_THRESHOLD_LEVEL3 = 50;
constexpr uint64_t BOTTLENECK_THRESHOLD_LEVEL4 = 100;

constexpr float PAGEACACHE_MIGRATION_RATIO_LEVEL1 = 0.1;
constexpr float PAGEACACHE_MIGRATION_RATIO_LEVEL2 = 0.4;
constexpr float PAGEACACHE_MIGRATION_RATIO_LEVEL3 = 0.7;
constexpr float PAGEACACHE_MIGRATION_RATIO_LEVEL4 = 1.0;

constexpr int KB_TO_BYTE = 1024;
constexpr int MB_TO_KB = 1024;

constexpr int MAX_UCACHE_USAGE_PERSENTAGE = 50;

constexpr int PERSENTAGE_NUM = 100;
} // namespace

void BottleneckDetector::Init()
{
    std::lock_guard<std::mutex> lock(initMutex_);
    if (initialized_) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] BottleneckDetector already initialized.";
        return;
    }

    initialized_ = true;
    detectionThread_ = std::thread(&BottleneckDetector::BottleneckDetectionLoop, this);
}

void BottleneckDetector::Deinit()
{
    std::lock_guard<std::mutex> lock(initMutex_);
    if (!initialized_) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] BottleneckDetector not initialized.";
        return;
    }
 
    stopScanning_ = true;
    if (detectionThread_.joinable()) {
        detectionThread_.join();
    }
 
    initialized_ = false;
}
 
void BottleneckDetector::BottleneckDetectionLoop()
{
    uint32_t ret = RMRS_OK;
    while (!stopScanning_) {
        ret = RunBottleneckDetection();
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Failed to RunBottleneckDetection, ret=" << ret << ".";
            containerInfoList_.clear();
            return;
        }
        std::this_thread::sleep_for(std::chrono::seconds(BOTTLENECK_DETECTION_INTERVAL));
    }
}

bool ReadContainerInactiveFile(const std::string &statPath, const std::string &id, uint64_t &totalInactiveFile,
                               float ratio)
{
    std::vector<std::string> fileLines{};
    RMRS_RES result = RmrsFileUtil::GetFileInfo(statPath, fileLines);
    if (result != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Failed to read memstat file of container, containerId=" << id << ".";
        return false;
    }
    bool foundInactiveFile = false;
    for (const auto &line : fileLines) {
        if (line.rfind("inactive_file", 0) == 0) {
            std::istringstream iss(line);
            std::string key{};
            uint64_t value{};
            if (!(iss >> key >> value)) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[UCache] Invalid line format, line=" << line << ".";
                break;
            }
            float temp = value * ratio;
                if (temp > static_cast<float>(UINT64_MAX) - totalInactiveFile) {
                    UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                        << "[UCache] inactive_file value too large, overflow risk, container=" << id << ".";
                    return false;
                }
                totalInactiveFile += static_cast<uint64_t>(temp);
                foundInactiveFile = true;
                UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[UCache] Found inactivefileKB=" << (value / KB_TO_BYTE) << ", for container=" << id << ".";
            break;
        }
    }

    if (!foundInactiveFile) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Failed to find container inactive_file, container=" << id << ".";
        return false;
    }

    return true;
}

float GetMigrationRatio(IoBottleneckLevel level)
{
    float ratio = 0;
    switch (level) {
        case IoBottleneckLevel::LEVEL1:
            ratio = PAGEACACHE_MIGRATION_RATIO_LEVEL1;
            break;
        case IoBottleneckLevel::LEVEL2:
            ratio = PAGEACACHE_MIGRATION_RATIO_LEVEL2;
            break;
        case IoBottleneckLevel::LEVEL3:
            ratio = PAGEACACHE_MIGRATION_RATIO_LEVEL3;
            break;
        case IoBottleneckLevel::LEVEL4:
            ratio = PAGEACACHE_MIGRATION_RATIO_LEVEL4;
            break;
        default:
            break;
    }
    return ratio;
}

uint32_t BottleneckDetector::GetTotalInactiveFilePages(const std::set<std::string> &containerIds,
                                                       uint64_t &totalInactiveFileKB)
{
    const std::string basePath = "/sys/fs/cgroup/memory/kubepods.slice/kubepods-besteffort.slice/";
    uint64_t totalInactiveFile = 0;
 
    for (const auto &id : containerIds) {
        auto it = std::find_if(containerInfoList_.begin(), containerInfoList_.end(),
                               [&id](const ContainerInfo &c) { return c.id == id; });
        if (it == containerInfoList_.end()) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Failed to get info of container, containerId=" << id << ".";
            return RMRS_ERROR;
        }
        const ContainerInfo &container = *it;

        if (!container.isActive || container.leneckLevel == IoBottleneckLevel::NOLOBOTTLENECK) {
            UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Container is not IObottleneck, containerId=" << id << ".";
            continue;
        }
        float ratio = GetMigrationRatio(container.leneckLevel);
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Get ratio=" << ratio << "IoBottleneckLevel=" << static_cast<int>(container.leneckLevel) << ".";
        std::string statPath = basePath + "cri-containerd-" + container.id + ".scope/memory.stat";
 
        if (!ReadContainerInactiveFile(statPath, id, totalInactiveFile, ratio)) {
            return RMRS_ERROR;
        }
    }
    totalInactiveFileKB = totalInactiveFile / KB_TO_BYTE;
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[UCache] Found total inactivefileKB=" << totalInactiveFileKB << ".";
    return RMRS_OK;
}
 
uint32_t BottleneckDetector::GetUCacheUsagePercentage(const rmrs::MigrationInfoParam &info,
                                                      uint32_t &ucacheUsagePercentage)
{
    if (info.borrowMemKB == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] No borrowMem.";
        return RMRS_ERROR;
    }
    // 解析入参pids对应的容器id列表
    std::set<std::string> containerIds{};
    uint32_t ret = GetContainerIdFromPids(info.pids, containerIds);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] Failed to GetContainerIdFromPids.";
        return ret;
    }
 
    // 查询容器列表的pagecache总大小
    uint64_t totalInactiveFileKB = 0;
    ret = GetTotalInactiveFilePages(containerIds, totalInactiveFileKB);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] Failed to GetTotalInactiveFilePages.";
        return ret;
    }
 
    // 对比借用内存大小，确定迁移比例
    ucacheUsagePercentage =
        static_cast<uint32_t>(static_cast<double>(totalInactiveFileKB) / info.borrowMemKB * PERSENTAGE_NUM);

    if (ucacheUsagePercentage > MAX_UCACHE_USAGE_PERSENTAGE) {
        ucacheUsagePercentage = MAX_UCACHE_USAGE_PERSENTAGE;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[UCache] All IO bottleneck containers totalInactiveFileKB=" << totalInactiveFileKB << ".";
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] Borrow_mem_in_KB=" << info.borrowMemKB << ".";
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[UCache] Set ucacheUsagePercentage=" << ucacheUsagePercentage << ".";
    return RMRS_OK;
}
 
uint32_t BottleneckDetector::RunBottleneckDetection()
{
    uint32_t ret = UpdateContainerList();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Failed to UpdateContainerList, ret=" << ret << ".";
        return ret;
    }
    IdentifyBottlenecks();
    return RMRS_OK;
}
 
void UpdateConatainer(ContainerInfo &cur, const ContainerInfo &prev, std::vector<ContainerInfo> &updatedList)
{
    // 计算 iowaitRatio pageCacheInMB ioReadBandwidthMB
    double sumRatio = 0.0;
    int count = 0;
 
    for (int cpu : cur.boundCpus) {
        auto prevIt = prev.cpuStats.find(cpu);
        auto curIt = cur.cpuStats.find(cpu);
        if (prevIt != prev.cpuStats.end() && curIt != cur.cpuStats.end()) {
            uint64_t prevIowait = prevIt->second.first;
            uint64_t prevTotal = prevIt->second.second;
            uint64_t curIowait = curIt->second.first;
            uint64_t curTotal = curIt->second.second;
 
            uint64_t deltaTotal = curTotal > prevTotal ? (curTotal - prevTotal) : 0;
            uint64_t deltaIowait = curIowait > prevIowait ? (curIowait - prevIowait) : 0;
 
            if (deltaTotal > 0) {
                double ratio = static_cast<double>(deltaIowait) / deltaTotal;
                sumRatio += ratio;
                ++count;
            }
        }
    }
    cur.iowaitRatio = (count > 0) ? (sumRatio / count) : 0.0;
    if (cur.ioReadBytes < prev.ioReadBytes || cur.pageCacheIn < prev.pageCacheIn) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Invalid ioReadBytes or pageCacheIn value, containerId=" << cur.id << ".";
        cur.ioReadBandwidthMB = 0;
        cur.pageCacheInMB = 0;
    } else {
        cur.ioReadBandwidthMB = (cur.ioReadBytes - prev.ioReadBytes) / (MB_TO_BYTE * BOTTLENECK_DETECTION_INTERVAL);
        cur.pageCacheInMB =
            (cur.pageCacheIn - prev.pageCacheIn) * PAGE_SIZE / (MB_TO_KB * BOTTLENECK_DETECTION_INTERVAL);
    }
    updatedList.push_back(cur);
}
 
void DealContainerWithSameId(ContainerInfo &cur, const ContainerInfo &prev, std::vector<ContainerInfo> &updatedList)
{
    if (IsSameContainer(cur, prev)) {
        UpdateConatainer(cur, prev, updatedList);
    } else {
        // id 相同但 boundCpus 不同，视为新容器
        updatedList.push_back(cur); // 替换为新容器
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] New container found, containerId=" << cur.id << ".";
    }
}
 
void BottleneckDetector::DoUpdateContainerList(std::vector<ContainerInfo> &curContainerInfoList,
                                               std::vector<ContainerInfo> &updatedList)
{
    for (auto &cur : curContainerInfoList) {
        bool matched = false;
        for (auto &prev : containerInfoList_) {
            if (cur.id == prev.id) {
                DealContainerWithSameId(cur, prev, updatedList);
                matched = true;
                break;
            }
        }
        if (!matched) {
            // 新容器
            updatedList.push_back(cur);
            UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] New container found, containerId=" << cur.id << ".";
        }
    }
}
 
uint32_t BottleneckDetector::UpdateContainerList()
{
    std::vector<ContainerInfo> curContainerInfoList{};
    uint32_t ret = GetCurContainerList(curContainerInfoList);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Failed to get curContainerInfoList, ret=" << ret << ".";
        return ret;
    } else {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Successfully get curContainerInfo for " << curContainerInfoList.size() << " containers.";
    }
 
    std::vector<ContainerInfo> updatedList{};
    DoUpdateContainerList(curContainerInfoList, updatedList);
 
    // 删除 cur 中不存在的容器（通过 id 检查）
    std::unordered_set<std::string> curIds;
    for (const auto &c : curContainerInfoList) {
        curIds.insert(c.id);
    }
    std::vector<ContainerInfo> finalList;
    for (const auto &c : updatedList) {
        if (curIds.count(c.id)) {
            finalList.push_back(c);
        }
    }
 
    containerInfoList_ = std::move(finalList);
    return RMRS_OK;
}
 
void BottleneckDetector::IdentifyBottlenecks()
{
    for (auto &container : containerInfoList_) {
        if (container.isActive && container.iowaitRatio > BOTTLENECK_THRESHOLD_IOWAIT) {
            if (container.pageCacheInMB > BOTTLENECK_THRESHOLD_LEVEL4 &&
                container.ioReadBandwidthMB > BOTTLENECK_THRESHOLD_LEVEL4) {
                UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[UCache] Found IO bottleneck container, containerId=" << container.id
                    << ", iowaitRatio=" << container.iowaitRatio << ".";
                container.leneckLevel = IoBottleneckLevel::LEVEL4;
            } else if (container.pageCacheInMB > BOTTLENECK_THRESHOLD_LEVEL3 &&
                       container.ioReadBandwidthMB > BOTTLENECK_THRESHOLD_LEVEL3) {
                UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[UCache] Found IO bottleneck container, containerId=" << container.id
                    << ", iowaitRatio=" << container.iowaitRatio << ".";
                container.leneckLevel = IoBottleneckLevel::LEVEL3;
            } else if (container.pageCacheInMB > BOTTLENECK_THRESHOLD_LEVEL2 &&
                       container.ioReadBandwidthMB > BOTTLENECK_THRESHOLD_LEVEL2) {
                UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[UCache] Found IO bottleneck container, containerId=" << container.id
                    << ", iowaitRatio=" << container.iowaitRatio << ".";
                container.leneckLevel = IoBottleneckLevel::LEVEL2;
            } else {
                UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[UCache] Found IO bottleneck container, containerId=" << container.id
                    << ", iowaitRatio=" << container.iowaitRatio << ".";
                container.leneckLevel = IoBottleneckLevel::LEVEL1;
            }
        } else {
            UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Found no IO bottleneck container, containerId=" << container.id
                << ", iowaitRatio=" << container.iowaitRatio << ".";
            container.leneckLevel = IoBottleneckLevel::NOLOBOTTLENECK;
        }
    }
}

uint32_t GetContainerIdFromPids(const std::vector<pid_t> &pids, std::set<std::string> &containerIds)
{
    std::regex idRegex(R"(cri-containerd-([a-f0-9]{64})\.scope)");
    for (pid_t pid : pids) {
        std::string cpusetPath = "/proc/" + std::to_string(pid) + "/cpuset";
        std::vector<std::string> fileLines{};
        RMRS_RES result = RmrsFileUtil::GetFileInfo(cpusetPath, fileLines);
        if (result != RMRS_OK || fileLines.empty()) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Failed to read cgroupfile of pid, pid=" << pid << ".";
            return RMRS_ERROR;
        }
        std::smatch match;
        if (std::regex_search(fileLines[0], match, idRegex) && match.size() == EXPECTED_MATCH_GROUPS) {
            containerIds.insert(match[1].str());
            UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Found containerid for pid, pid=" << pid << ", containerid=" << match[1].str() << ".";
        } else {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Unexpected cpuset format for pid, pid=" << pid << ", line=" << fileLines[0] << ".";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}
 
bool IsSameContainer(const ContainerInfo &a, const ContainerInfo &b)
{
    if (a.id != b.id) {
        return false;
    }
    if (a.boundCpus.size() != b.boundCpus.size()) {
        return false;
    }
    std::vector<int> aCpus = a.boundCpus;
    std::vector<int> bCpus = b.boundCpus;
    std::sort(aCpus.begin(), aCpus.end());
    std::sort(bCpus.begin(), bCpus.end());
 
    return aCpus == bCpus;
}

bool IsPauseCommand(const std::string &cmd)
{
    const std::string pauseCommand = "/pause";
    std::string s = cmd;
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::iscntrl(c); }), s.end());
    return s == pauseCommand;
}

bool IsContainerActive(const std::string &containerId)
{
    std::string tasksPath = K8S_CONTAINER_PATH + "cri-containerd-" + containerId + ".scope" + "/tasks";
    std::vector<std::string> fileLines{};
    RMRS_RES result = RmrsFileUtil::GetFileInfo(tasksPath, fileLines);
    if (result != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Failed to read tasks file, containerId=" << containerId << ".";
        return false;
    }
    if (fileLines.empty()) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Tasks file is empty, containerId=" << containerId << ".";
        return false;
    }
    std::vector<std::string> pids;
    for (const auto &line : fileLines) {
        if (!line.empty()) {
            pids.push_back(line);
        }
    }
    if (pids.empty()) {
        return false;
    }
    if (pids.size() == 1) {
        std::string cmdlinePath = "/proc/" + pids[0] + "/cmdline";
        std::vector<std::string> cmdlineLines{};
        result = RmrsFileUtil::GetFileInfo(cmdlinePath, cmdlineLines);
        if (result != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Failed to read cmdline file, containerId=" << containerId << ".";
            return false;
        }
        if (!cmdlineLines.empty() && IsPauseCommand(cmdlineLines[0])) {
            return false;
        }
    }
    return true;
}
 
bool ParseCpuSetSegment(const std::string &segment, ContainerInfo &container)
{
    try {
        size_t dash = segment.find('-');
        if (dash != std::string::npos) {
            int start = std::stoi(segment.substr(0, dash));
            int end = std::stoi(segment.substr(dash + 1));
            for (int i = start; i <= end; ++i) {
                container.boundCpus.push_back(i);
            }
        } else {
            container.boundCpus.push_back(std::stoi(segment));
        }
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Invalid CPU segment, segment=" << segment << ".";
        return false;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] CPU value out of range, segment=" << segment << ".";
        return false;
    }
    return true;
}
 
uint32_t ExtractContainerCpuset(ContainerInfo &container)
{
    if (!container.isActive) {
        return RMRS_OK;
    }
    std::string cpusetPath = K8S_CONTAINER_PATH + "cri-containerd-" + container.id + ".scope" + "/cpuset.cpus";
    std::vector<std::string> fileLines;
    RMRS_RES result = RmrsFileUtil::GetFileInfo(cpusetPath, fileLines);
    if (result != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Failed to read cpuset file, containerId=" << container.id << ".";
        return RMRS_ERROR;
    }
    if (fileLines.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Cpuset file is empty, containerId=" << container.id << ".";
        return RMRS_ERROR;
    }
    std::string line = fileLines[0];
    std::stringstream ss(line);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        if (!ParseCpuSetSegment(segment, container)) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Failed to parse cpuset segment, containerId=" << container.id << ".";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}
 
bool ParseContainerCpuStatLine(const std::string &line, std::map<int, std::pair<uint64_t, uint64_t>> &systemCpuStats)
{
    std::istringstream iss(line);
    std::string cpuLabel{};
    uint64_t user{};
    uint64_t nice{};
    uint64_t system{};
    uint64_t idle{};
    uint64_t iowait{};
    uint64_t irq{};
    uint64_t softirq{};
    uint64_t steal{};
    if (!(iss >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) ||
        cpuLabel.length() < MIN_CPU_STR_LENGTH) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Invalid format in cpuStatPath line, line=" << line << ".";
        return false;
    }
    int cpuId = -1;
    try {
        cpuId = std::stoi(cpuLabel.substr(CPU_INDEX_START)); // 提取"cpuX"中的X
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Invalid CPU cpuLabel, cpuLabel=" << cpuLabel << ".";
        return false;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] CpuLabel value out of range, cpuLabel=" << cpuLabel << ".";
        return false;
    }
    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
    systemCpuStats[cpuId] = {iowait, total};
    return true;
}

uint32_t ExtractContainerCpuStat(std::vector<ContainerInfo> &containerInfoList)
{
    const std::string cpuStatPath = "/proc/stat";
    std::vector<std::string> fileLines{};
    RMRS_RES result = RmrsFileUtil::GetFileInfo(cpuStatPath, fileLines);
    if (result != RMRS_OK || fileLines.empty()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] Failed to open cpuStatPath.";
        return RMRS_ERROR;
    }
    std::map<int, std::pair<uint64_t, uint64_t>> systemCpuStats{};
    for (auto line : fileLines) {
        // 跳过非cpu行
        if (line.rfind("cpu", 0) != 0) {
            continue;
        }
        // 跳过总cpu行（注意总cpu行是 "cpu " 后面有空格）
        if (line.rfind("cpu ", 0) == 0) {
            continue;
        }
        uint64_t iowait{};
        uint64_t total{};
        int cpuId = -1;
        if (!ParseContainerCpuStatLine(line, systemCpuStats)) {
            return RMRS_ERROR;
        }
    }
    // 遍历每个容器，只记录其绑定 CPU 的统计数据
    for (auto &container : containerInfoList) {
        for (int cpu : container.boundCpus) {
            auto it = systemCpuStats.find(cpu);
            if (it != systemCpuStats.end()) {
                container.cpuStats[cpu] = it->second;
            }
        }
    }
    return RMRS_OK;
}

void GetCpusetInfo(std::vector<ContainerInfo> &containerInfoList)
{
    DIR *dir = opendir(K8S_CONTAINER_PATH.c_str());
    if (!dir) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[UCache] Failed to open K8S_CONTAINER_PATH, err=" << strerror(errno) << ".";
        return;
    }

    struct dirent *entry = nullptr;
    std::regex pattern(R"(cri-containerd-([a-f0-9]{64})\.scope)");
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR && entry->d_type != DT_LNK) {
            continue;
        }
        std::string dirName(entry->d_name);
        std::smatch match;
        if (!std::regex_match(dirName, match, pattern)) {
            continue;
        }

        if (match.size() != EXPECTED_MATCH_GROUPS) { // 第一个是完整匹配，第二个是捕获的 id
            continue;
        }

        ContainerInfo info{};
        info.id = match[1].str();
        if (!IsContainerActive(info.id)) {
            continue;
        }
        info.isActive = true;

        uint32_t ret = ExtractContainerCpuset(info);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Get cpulist failed, containerId=" << info.id << ".";
            closedir(dir);
            return;
        }
        containerInfoList.push_back(info);
    }
    closedir(dir);
    return;
}

bool GetIoBytesInfo(std::vector<ContainerInfo> &containerInfoList)
{
    const std::string basePath = "/sys/fs/cgroup/blkio/kubepods.slice/kubepods-besteffort.slice/";
    for (auto &container : containerInfoList) {
        std::string statPath =
            basePath + "cri-containerd-" + container.id + ".scope/blkio.throttle.io_service_bytes_recursive";
        std::vector<std::string> fileLines;
        RMRS_RES result = RmrsFileUtil::GetFileInfo(statPath, fileLines);
        if (result != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Failed to read iostat file of container, containerId=" << container.id << ".";
            return false;
        }
        std::regex readRegex(R"(Read (\d+))");
        std::smatch readMatch;
        for (const auto &line : fileLines) {
            if (std::regex_search(line, readMatch, readRegex) && readMatch.size() >= EXPECTED_MATCH_GROUPS) {
                container.ioReadBytes = RmrsStringUtil::SafeStoull(readMatch[1]);
                break;
            }
        }
    }
    return true;
}

bool GetPageCacheInfo(std::vector<ContainerInfo> &containerInfoList)
{
    const std::string basePath = "/sys/fs/cgroup/memory/kubepods.slice/kubepods-besteffort.slice/";
    for (auto &container : containerInfoList) {
        std::string statPath = basePath + "cri-containerd-" + container.id + ".scope/memory.stat";
        std::vector<std::string> fileLines{};
        RMRS_RES result = RmrsFileUtil::GetFileInfo(statPath, fileLines);
        if (result != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[UCache] Failed to read iostat file of container, containerId=" << container.id << ".";
            return false;
        }
        std::regex totalPginRegex(R"(total_pgpgin (\d+))");
        std::smatch match;
        for (const auto &line : fileLines) {
            if (std::regex_search(line, match, totalPginRegex) && match.size() >= EXPECTED_MATCH_GROUPS) {
                container.pageCacheIn = RmrsStringUtil::SafeStoull(match[1]);
                break;
            }
        }
    }
    return true;
}

uint32_t GetCurContainerList(std::vector<ContainerInfo> &containerInfoList)
{
    GetCpusetInfo(containerInfoList);
    if (containerInfoList.size() == 0) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] No k8s containers found.";
        return RMRS_OK;
    }

    if (!(GetIoBytesInfo(containerInfoList)) || !GetPageCacheInfo(containerInfoList)) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] Get io bytes or page cache info failed.";
        return RMRS_ERROR;
    }

    uint32_t ret = ExtractContainerCpuStat(containerInfoList);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[UCache] Get cpu stat failed.";
        return ret;
    }
    return RMRS_OK;
}

} // namespace bottleneck_detector
} // namespace ucache