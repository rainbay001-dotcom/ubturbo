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
#include "ucache_migration_executor.h"

#include <dirent.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>

#include "rmrs_file_util.h"
#include "rmrs_config.h"
#include "rmrs_error.h"
#include "turbo_logger.h"
#include "ucache_driver_interaction.h"

namespace ucache {
using namespace turbo::log;
using namespace rmrs;
using namespace migrate_excutor;

const int PAGE_SIZE_KB = 4;

UcacheMigrationExecutor::~UcacheMigrationExecutor()
{
    UcacheMigrationExecutor::StopMigrationWorker();
}

uint32_t UcacheMigrationExecutor::ExecuteNewMigrationStrategy(const MigrationStrategy &newStrategy)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Execute new migration strategy.";
    if (alreadyStarting_) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Already starting a migration strategy, too frequent requests.";
        return RMRS_ERROR;
    }
    alreadyStarting_ = true;
    targetUsageRatio_ = newStrategy.ucacheUsageRatio;
    // 如果远端内存已达到使用水线，忽略该策略
    if (IsRemoteWatermarkExceeded(newStrategy)) {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Already satisfied this strategy, ignoring this ...";
        alreadyStarting_ = false;
        return RMRS_OK;
    }
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Change to the new migration strategy.";
    StopCurrentMigrationStrategy();

    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Check pids to migrate.";
    uint32_t ret = RecordPids(newStrategy);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Failed to get all pids.";
    }

    // 策略输入的所有pid都不是容器进程
    if (pidsToMigrate_.empty()) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] No pids to migrate, stop migration.";
        alreadyStarting_ = false;
        return RMRS_OK;
    }

    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Setting new migration strategy.";
    {
        std::lock_guard<std::mutex> lock(actionMutex_);
        currentStrategy_ = newStrategy;
    }
    shouldRun_.store(true);

    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Starting migration worker thread.";

    StartMigrationWorker();
    alreadyStarting_ = false;
    return RMRS_OK;
}

uint32_t UcacheMigrationExecutor::StopCurrentMigrationStrategy()
{
    shouldRun_.store(false);
    if (migrationWorkerThread_.joinable()) {
        migrationWorkerThread_.join();
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Stopped current migration strategy successfully.";
    } else {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] No current migration strategy to stop.";
    }
    return RMRS_OK;
}

void UcacheMigrationExecutor::StartMigrationWorker()
{
    migrationWorkerThread_ = std::thread([this]() { MigrationWorkerTask(); });
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Migration worker thread started.";
}

void UcacheMigrationExecutor::StopMigrationWorker()
{
    shouldRun_.store(false);
    if (migrationWorkerThread_.joinable()) {
        migrationWorkerThread_.join();
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Old migration worker thread stopped successfully.";
    } else {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] No migration worker thread to stop.";
    }
}

void UcacheMigrationExecutor::MigrationWorkerTask()
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Start Migration worker task.";
    while (shouldRun_) {
        if (!currentStrategy_.has_value()) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] No currentStrategy_, worker task stopped.";
            return;
        }

        PerformMigration(currentStrategy_.value());
    }
}

// success表示迁移成功的页数
uint32_t UcacheMigrationExecutor::ExcuteMigrateFolios(const uint16_t desNid, const int16_t srcNid, const pid_t pid,
                                                      uint32_t &success)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Execute migrate folios, pid=" << pid << ", desNid=" << desNid << ", srcNid=" << srcNid << ".";

    if (IsNidWatermarkExceeded(desNid)) {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Already satisfied this strategy, desNid=" << desNid << ".";
        return RMRS_OK;
    }
    if (!shouldRun_) {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Stop current migration task";
        return RMRS_OK;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(migrateInterval_));
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Call migrate_folios, pid=" << pid << ", srcNid=" << srcNid << ", desNid=" << desNid << ".";
    uint32_t ret = DriverInteraction::GetInstance().MigrateFoliosInfo(desNid, static_cast<int32_t>(srcNid), pid);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to migrate folios info, ret=" << ret << ".";
        shouldRun_.store(false);
        return ret;
    }
    struct MigrateSuccess queryArg = {
        .nid = desNid,
    };
    ret = DriverInteraction::GetInstance().GetMigrateSuccess(queryArg);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to get migrate success, ret=" << ret << ".";
        shouldRun_.store(false);
        return ret;
    }
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Query migrate folios to nid=" << desNid << ", success=" << queryArg.nrSucceeded << ".";
    success = queryArg.nrSucceeded;
    return RMRS_OK;
}

// 迁移单个本地numa
uint32_t UcacheMigrationExecutor::MigrateFromSingleSrcNid(const uint16_t desNid, const int16_t srcNid, const pid_t pid)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Strat to migrate from single srcNid=" << srcNid << ", to desNid=" << desNid << ".";
    uint32_t ret = RMRS_OK;
    uint32_t success = 0;

    ret = ExcuteMigrateFolios(desNid, srcNid, pid, success);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Failed to excute migrate folios.";
        return ret;
    }
    if (success == 0) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Migrate folios no success.";
        return RMRS_WARN;
    } else {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Migrate folios success count=" << success << ".";
        return RMRS_OK;
    }
}

// 迁移所有本地numa
uint32_t UcacheMigrationExecutor::MigrateFromMultipleSrcNids(const uint16_t desNid, const pid_t pid)
{
    // 获取所有本地numa节点
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Migration pid=" << pid << " to desNid=" << desNid << " from all srcNids.";
    std::vector<int16_t> srcNids;
    uint32_t ret = GetLocalNumaIDs(srcNids);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Failed to get local numa ids.";
        return ret;
    }
    // 所有本地numa都迁移页数为零，认为该进程没有冷页
    bool migrateSuccess = false;
    for (const auto &nid : srcNids) {
        ret = MigrateFromSingleSrcNid(desNid, nid, pid);
        if (ret == RMRS_ERROR) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Failed to migrate from single srcNid=" << nid << ".";
            return ret;
        } else if (ret == RMRS_OK) {
            migrateSuccess = true;
        }
    }
    if (migrateSuccess) {
        return RMRS_OK;
    } else {
        return RMRS_WARN;
    }
}

// 向单个远端numa节点迁移内存
void UcacheMigrationExecutor::MigrateToRemoteNode(const uint16_t desNid, const MigrationStrategy &strategy)
{
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[ucache] Start to migrate to remote node=" << desNid << ".";
    // 如果远端内存已经满足使用水线
    if (IsNidWatermarkExceeded(desNid)) {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Already satisfied this strategy, desNid=" << desNid << ".";
        return;
    }
    uint32_t ret = RMRS_OK;
    for (const auto &pid : pidsToMigrate_) {
        // srcNid小于零表示从所有本地numa节点迁移
        if (strategy.srcNid < 0) {
            ret = MigrateFromMultipleSrcNids(desNid, pid);
        } else {
            ret = MigrateFromSingleSrcNid(desNid, strategy.srcNid, pid);
        }
        if (ret == RMRS_ERROR) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Failed to migrate pid=" << pid << ".";
            return;
        } else if (ret == RMRS_WARN) { // 没有冷页的进程从迁移进程组里删除
            UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Failed to scan migrate folios from pid=" << pid << ".";
            pidsToMigrate_.erase(std::remove(pidsToMigrate_.begin(), pidsToMigrate_.end(), pid), pidsToMigrate_.end());
        }
    }
}

void UcacheMigrationExecutor::PerformMigration(const MigrationStrategy &strategy)
{
    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Perform migration start.";
    if (pidsToMigrate_.empty()) {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] No pids to migrate, stopping migration.";
        shouldRun_.store(false);
        return;
    }

    if (IsRemoteWatermarkExceeded(strategy)) {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Already satisfied this strategy, stopping migration.";
        shouldRun_.store(false);
        return;
    }

    for (auto &nid : strategy.desNids) {
        MigrateToRemoteNode(nid, strategy);
    }
}

static uint32_t RegexNodeNum(std::string name, std::smatch match, std::regex node_regex, std::vector<int16_t> &nids)
{
    const std::string basePath = "/sys/devices/system/node/";
    // 匹配 nodeX 格式
    if (std::regex_match(name, match, node_regex)) {
        if (match.size() <= 1) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Regex match numa system_path failed.";
            return RMRS_ERROR;
        }
        // 提取数字部分
        std::string numaStr = match[1].str();
        try {
            int16_t numaId = std::stoi(numaStr);
            std::string remotePath = basePath + "node" + numaStr + "/remote";

            // 读取 remote 文件
            std::vector<std::string> fileLines;
            RMRS_RES result = RmrsFileUtil::GetFileInfo(remotePath, fileLines);
            if (result != RMRS_OK) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] Failed to open remote file of numaId=" << numaId << ".";
                return RMRS_ERROR;
            }

            if (fileLines.empty()) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] Remote file is empty: " << remotePath << ".";
                return RMRS_ERROR;
            }
            std::string remoteValueStr = fileLines[0];
            int remoteValue = std::stoi(remoteValueStr);
            if (remoteValue == 0) {
                nids.push_back(numaId);
            }
        } catch (const std::invalid_argument &e) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Invalid NUMA ID format, numaStr=" << numaStr << ".";
            return RMRS_ERROR;
        } catch (const std::out_of_range &e) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] NUMA ID out of range, numaStr=" << numaStr << ".";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

uint32_t UcacheMigrationExecutor::GetLocalNumaIDs(std::vector<int16_t> &nids)
{
    nids.clear();
    const std::string basePath = "/sys/devices/system/node/";

    // 打开目录
    DIR *dir = opendir(basePath.c_str());
    if (!dir) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Failed to open node system_path.";
        return RMRS_ERROR;
    }

    // 正则表达式匹配 nodeX 格式的文件夹
    std::regex node_regex("^node([0-9]+)$");

    // 遍历目录
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        std::smatch match;

        uint32_t ret = RegexNodeNum(name, match, node_regex, nids);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Failed to regex match name=" << name << ".";
            closedir(dir);
            return ret;
        }
    }

    UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Local node IDs: ";
    for (auto nid : nids) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] nid=" << nid << ".";
    }
    closedir(dir);
    return RMRS_OK;
}

bool UcacheMigrationExecutor::IsRemoteWatermarkExceeded(const MigrationStrategy &strategy)
{
    // 所有远端节点的水线都满足
    for (const auto &nid : strategy.desNids) {
        if (!IsNidWatermarkExceeded(nid)) {
            return false;
        }
    }
    return true;
}

bool UcacheMigrationExecutor::IsNidWatermarkExceeded(const uint16_t nid)
{
    uint64_t remoteMemTotal = GetMemTotalForNode(nid);
    if (remoteMemTotal == 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to get remote memory total for nid=" << nid << ".";
        return true;
    }
    uint64_t remoteMemFilePages = GetMemFilePagesForNode(nid);
    return static_cast<float>(remoteMemFilePages) / static_cast<float>(remoteMemTotal) >= targetUsageRatio_;
}

uint64_t UcacheMigrationExecutor::GetMemTotalForNode(uint16_t nid)
{
    std::string path = "/sys/devices/system/node/node" + std::to_string(nid) + "/meminfo";
    std::ifstream file(path);
    if (!file.is_open()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to open system_path of nodeId=" << nid << ".";
        return 0;
    }
    std::string line;
    std::regex memFreeRegex(R"(MemTotal:\s*(\d+))");
    std::smatch match;
    while (std::getline(file, line)) {
        if (std::regex_search(line, match, memFreeRegex)) {
            if (match.size() <= 1) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] Regex match memTotal failed, for numaId=" << nid << ".";
                return 0;
            }
            try {
                uint64_t value = std::stoull(match[1]);
                file.close();
                UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] Node=" << nid << " total_memory=" << value << ".";
                return value;
            } catch (const std::invalid_argument &e) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] Failed to parse meminfo, value=" << match[1] << ".";
            } catch (const std::out_of_range &e) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] meminfo value out of range, value=" << match[1] << ".";
            }
            break;
        }
    }
    file.close();
    return 0;
}

uint64_t UcacheMigrationExecutor::GetMemFilePagesForNode(uint16_t nid)
{
    std::string path = "/sys/devices/system/node/node" + std::to_string(nid) + "/meminfo";
    std::ifstream file(path);
    if (!file.is_open()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to open system_path of numaId=" << nid << ".";
        return 0;
    }
    std::string line;
    std::regex memFreeRegex(R"(FilePages:\s*(\d+))");
    std::smatch match;
    while (std::getline(file, line)) {
        if (std::regex_search(line, match, memFreeRegex)) {
            if (match.size() <= 1) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] Regex match memTotal failed, for numaId=" << nid << ".";
                return 0;
            }
            try {
                uint64_t value = std::stoull(match[1]);
                file.close();
                UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] NumaId=" << nid << ", file_pages_memory=" << value << ".";
                return value;
            } catch (const std::invalid_argument &e) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] Failed to parse meminfo, value=" << match[1] << ".";
            } catch (const std::out_of_range &e) {
                UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                    << "[ucache] meminfo value out of range, value=" << match[1] << ".";
            }
            break;
        }
    }
    file.close();
    return 0;
}

// 没找到返回""
std::string UcacheMigrationExecutor::GetContainerdIdByPid(const pid_t pid)
{
    constexpr int bufferSize = 128;
    std::string command = " cat /proc/" + std::to_string(pid) + "/cgroup | grep kubepods.slice | cut -d'/' -f4";
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Failed to find file of pid, pid=" << pid << ".";
        return "";
    }

    char buffer[bufferSize];
    std::string pidStr;

    if (fgets(buffer, sizeof(buffer), pipe)) {
        // 去除换行符
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        pidStr = buffer;
    } else {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Error reading from pipe.";
    }

    int ret = pclose(pipe);
    if (ret < 0) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Failed to close pipe.";
    }

    if (pidStr.empty()) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[ucache] Process is not in the containerd, pid =" << pid << ".";
    }
    return pidStr;
}

uint32_t UcacheMigrationExecutor::RecordPids(const MigrationStrategy &strategy)
{
    pidsToMigrate_.clear();
    for (const pid_t &pid : strategy.pids) {
        std::string containerdId = GetContainerdIdByPid(pid);
        if (containerdId != "") {
            pidsToMigrate_.push_back(pid);
            UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Found pid=" << pid << " for containerd=" << containerdId << ".";
        } else {
            UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[ucache] Process pid=" << pid << "is not in any containerd.";
        }
    }
    for (auto &pid : pidsToMigrate_) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[ucache] Added pid=" << pid << "to migration task.";
    }
    return RMRS_OK;
}

} // namespace ucache