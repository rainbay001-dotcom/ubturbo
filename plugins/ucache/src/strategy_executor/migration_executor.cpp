/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * ucache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "migration_executor.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>

#include "driver_interaction.h"
#include "turbo_logger.h"
#include "ucache_turbo_config.h"
#include "ucache_turbo_error.h"
#include "ucache_string_util.h"
#include "ucache_file_util.h"

namespace turbo::ucache {
using namespace turbo::log;

const int PAGE_SIZE_KB = 4;
const int MIN_TOTAL_MEM_PER_NUMA_KB = 1 * 1024 * 1024;
const int MIN_PAGE_CACHE_PER_NUMA_KB = 40 * 1024;

MigrationExecutor::~MigrationExecutor()
{
    MigrationExecutor::StopMigrationWorker();
}

static uint64_t GetNodeMemInfoByKey(int32_t nid, const char *key)
{
    std::string path = "/sys/devices/system/node/node" + std::to_string(nid) + "/meminfo";

    std::vector<std::string> lines;
    if (UCacheFileUtil::GetFileInfo(path, lines) != UCACHE_OK) {
        return 0;
    }

    for (const auto &line : lines) {
        uint64_t val = 0;
        if (TryExtractValue(line, key, val)) {
            return val;
        }
    }

    return 0;
}

uint64_t GetFilePagesForNode(int32_t nid)
{
    return GetNodeMemInfoByKey(nid, "FilePages:");
}

uint64_t GetMemTotalForNode(int32_t nid)
{
    return GetNodeMemInfoByKey(nid, "MemTotal:");
}

uint64_t GetMemFreeForNode(int32_t nid)
{
    return GetNodeMemInfoByKey(nid, "MemFree:");
}

uint32_t MigrationExecutor::ExecuteNewMigrationStrategy(const MigrationStrategy &newStrategy)
{
    if (alreadyStarting_) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Already starting a migration strategy, too frequent requests.";
        return UCACHE_ERR;
    }
    alreadyStarting_ = true;
    // 如果已经满足策略水线要求，忽略该策略
    if (IsLowWatermarkExceeded(newStrategy)) {
        UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Already satisfied this strategy, ignoring this ...";
        alreadyStarting_ = false;
        return UCACHE_OK;
    }
    UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Starting new migration strategy";
    StopCurrentMigrationStrategy();

    UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Getting docker_pids to migrate";
    uint32_t ret = RecordPids(newStrategy);
    if (ret != UCACHE_OK) {
        UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to get all docker_pids";
    }

    // 策略原因没找到可执行容器，停止当前任务返回
    if (pidsToMigrate_.empty()) {
        UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "No docker_pids to migrate, stop migration";
        alreadyStarting_ = false;
        return UCACHE_OK;
    }

    UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Setting new migration strategy";
    {
        std::lock_guard<std::mutex> lock(actionMutex_);
        currentStrategy_ = newStrategy;
    }
    shouldRun_.store(true);

    // 清除不满足条件的SrcNID
    auto it = currentStrategy_.value().srcNids.begin();
    while (it != currentStrategy_.value().srcNids.end()) {
        uint64_t totalMem = GetMemTotalForNode(*it);
        UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "totalMem for node" << *it << " is " << totalMem;
        if (totalMem < MIN_TOTAL_MEM_PER_NUMA_KB) {
            UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "totalMem for node" << *it << " below the limit MIN_TOTAL_MEM_PER_NUMA_KB";
            it = currentStrategy_.value().srcNids.erase(it);
        } else {
            ++it;
        }
    }

    UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Starting migration worker thread";

    StartMigrationWorker();
    alreadyStarting_ = false;
    return UCACHE_OK;
}

void MigrationExecutor::StopCurrentMigrationStrategy()
{
    shouldRun_.store(false);
    if (migrationWorkerThread_.joinable()) {
        migrationWorkerThread_.join();
        UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Stopped current migration strategy successfully";
    } else {
        UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "No current migration strategy";
    }
}

void MigrationExecutor::StartMigrationWorker()
{
    migrateInterval_ = UcacheTurboConfig::GetInstance().GetMigrateInterval();
    migrationWorkerThread_ = std::thread([this]() { MigrationWorkerTask(); });
    UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Migration worker thread started";
}

void MigrationExecutor::StopMigrationWorker()
{
    shouldRun_.store(false);
    if (migrationWorkerThread_.joinable()) {
        migrationWorkerThread_.join();
        UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Old migration worker thread stopped successfully";
    } else {
        UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "No migration worker thread to stop";
    }
}

void MigrationExecutor::MigrationWorkerTask()
{
    while (shouldRun_) {
        if (!currentStrategy_.has_value()) {
            UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "No currentStrategy_, worker task stopped";
            return;
        }

        if (IsHighWatermarkExceeded(currentStrategy_.value())) {
            UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "HighWatermarkExceeded, currentStrategy_ task finished";
            return;
        }

        PerformMigration(currentStrategy_.value());
    }
}

bool MigrationExecutor::IsLowWatermarkExceeded(const MigrationStrategy &strategy)
{
    return GetLocalMemFreeKB(strategy) > strategy.lowWatermarkPages * PAGE_SIZE_KB;
}

bool MigrationExecutor::IsHighWatermarkExceeded(const MigrationStrategy &strategy)
{
    return GetLocalMemFreeKB(strategy) > strategy.highWatermarkPages * PAGE_SIZE_KB;
}

void MigrationExecutor::PerformMigration(const MigrationStrategy &strategy)
{
    for (const auto &pid : pidsToMigrate_) {
        for (const auto &srcNid : strategy.srcNids) {
            if (!shouldRun_) {
                UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Stop current migration task";
                return;
            }
            uint64_t filePages = GetFilePagesForNode(srcNid);
            if (filePages < MIN_PAGE_CACHE_PER_NUMA_KB) {
                std::this_thread::sleep_for(std::chrono::milliseconds(migrateInterval_));
                UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                    << "FilePages below the limit MIN_PAGE_CACHE_PER_NUMA_KB" << ", srcNid=" << srcNid
                    << ", filePages=" << filePages << ".";
                continue;
            }
            uint32_t ret = DriverInteraction::GetInstance().MigrateFoliosInfo(strategy.dstNid, srcNid, pid);
            if (ret != UCACHE_OK) {
                UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                    << "Failed to migrate folios info, ret=" << ret;
                shouldRun_.store(false);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(migrateInterval_));
        }
    }
}

uint64_t MigrationExecutor::GetLocalMemFreeKB(const MigrationStrategy &strategy)
{
    uint64_t localMemFree = 0;
    for (auto nid : strategy.srcNids) {
        uint64_t memFreeNode = GetMemFreeForNode(nid);
        localMemFree += memFreeNode;
        if (memFreeNode == 0) {
            UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node " << nid << " has no MemFree";
        }
    }
    if (localMemFree == 0) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to get local memory free from /proc/meminfo";
        shouldRun_ = false;
    }
    return localMemFree;
}

bool IsContainerInit(pid_t pid)
{
    const size_t nsPidPrefixLen = 6;
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(path);
    if (!file.is_open()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to open procFile, path=" << path << ".";
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("NSpid:", 0) == 0) {
            std::istringstream iss(line.substr(nsPidPrefixLen));
            int nsPid;
            int last = -1;
            while (iss >> nsPid) {
                last = nsPid;
            }
            return last == 1;
        }
    }
    return false;
}

pid_t MigrationExecutor::GetDockerPid(const std::string &dockerId)
{
    std::string path = "/sys/fs/cgroup/memory/docker/" + dockerId + "/tasks";
    std::ifstream file(path);
    if (!file.is_open()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to open dockerFile, path=" << path << ".";
        return -1;
    }

    pid_t pid;
    while (file >> pid) {
        if (IsContainerInit(pid)) {
            return pid; // 找到主进程
        }
    }
    UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to open dockerFile, path=" << path << ".";
    return -1;
}

uint32_t MigrationExecutor::RecordPids(const MigrationStrategy &strategy)
{
    pidsToMigrate_.clear();
    for (const std::string &dockerId : strategy.dockerIds) {
        pid_t pid = GetDockerPid(dockerId);
        if (pid > 0) {
            pidsToMigrate_.push_back(pid);
            UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Found PID " << pid << " for container " << dockerId;
        } else {
            UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to get PID for container " << dockerId;
            return UCACHE_ERR;
        }
    }
    return UCACHE_OK;
}

} // namespace turbo::ucache