/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#ifndef BOTTLENECK_DETECTOR_H
#define BOTTLENECK_DETECTOR_H

#include <sys/types.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "turbo_ucache_message.h"

namespace ucache {
namespace bottleneck_detector {

const uint64_t MB_TO_BYTE = 1024 * 1024;
const int PAGE_SIZE = 4; // 单位为KB
const int BOTTLENECK_DETECTION_INTERVAL = 10;

enum class IoBottleneckLevel {
    NOLOBOTTLENECK, // 无瓶颈
    LEVEL1,         // 其它情况，容器为1级IO瓶颈   10%
    LEVEL2,         // pagecache生成速率、IO读带宽均大于10MB/s，容器为2级IO瓶颈   40%
    LEVEL3,         // pagecache生成速率、IO读带宽均大于50MB/s，容器为3级IO瓶颈   70%
    LEVEL4          // pagecache生成速率、IO读带宽均大于100MB/s，容器为4级IO瓶颈  100%
};

struct ContainerInfo {
    std::string id;
    std::vector<int> boundCpus;                            // 绑定的CPU列表
    std::map<int, std::pair<uint64_t, uint64_t>> cpuStats; // {cpu_id: {last_iowait, last_total_time}}
    bool isActive = false;                                 // 是否活跃容器
    double iowaitRatio = 0.0;                              // 当前iowait比率
    IoBottleneckLevel leneckLevel;                         // io瓶颈级别
    uint64_t pageCacheIn = 0;                              // 页面从磁盘交换到内存的次数
    uint64_t ioReadBytes = 0;                              // io读字节数
    uint64_t pageCacheInMB = 0;                            // pagecache 生成速度，单位MB/s
    uint64_t ioReadBandwidthMB = 0;                        // io读带宽，单位MB/s
};

uint32_t GetCurContainerList(std::vector<ContainerInfo> &containerInfoList);
uint32_t ExtractContainerCpuStat(std::vector<ContainerInfo> &containerInfoList);
uint32_t ExtractContainerCpuset(ContainerInfo &container);
bool IsContainerActive(const std::string &containerId);
bool IsSameContainer(const ContainerInfo &a, const ContainerInfo &b);
uint32_t GetContainerIdFromPids(const std::vector<pid_t> &pids, std::set<std::string> &containerIds);

class BottleneckDetector {
public:
    static BottleneckDetector &GetInstance()
    {
        static BottleneckDetector instance;
        return instance;
    }
    BottleneckDetector(const BottleneckDetector &) = delete;
    BottleneckDetector &operator=(const BottleneckDetector &) = delete;

    uint32_t GetUCacheUsagePercentage(const rmrs::MigrationInfoParam &info, uint32_t &ucacheUsagePercentage);

    void Init();
    void Deinit();

private:
    BottleneckDetector() : stopScanning_(false), detectionThread_(), initMutex_(), initialized_(false) {}
    ~BottleneckDetector()
    {
        Deinit();
    }

    void BottleneckDetectionLoop();
    uint32_t RunBottleneckDetection();
    uint32_t UpdateContainerList();
    void DoUpdateContainerList(std::vector<ContainerInfo> &curContainerInfoList,
                               std::vector<ContainerInfo> &updatedList);
    void IdentifyBottlenecks();
    uint32_t GetTotalInactiveFilePages(const std::set<std::string> &containerIds, uint64_t &totalInactiveFileKB);

    std::atomic<bool> stopScanning_;
    std::thread detectionThread_;
    std::mutex initMutex_;
    std::atomic<bool> initialized_;
    std::vector<ContainerInfo> containerInfoList_{};
};

} // namespace bottleneck_detector

} // namespace ucache

#endif