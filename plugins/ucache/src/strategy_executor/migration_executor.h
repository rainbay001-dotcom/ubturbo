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

#ifndef MIGRATION_EXECUTOR_H
#define MIGRATION_EXECUTOR_H

#include <sys/types.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <cstring>
#include <thread>
#include <vector>

namespace turbo::ucache {

struct MigrationStrategy {
    int32_t dstNid;                     // 迁出目标节点（远端内存）
    uint64_t highWatermarkPages;        // 本地高水线，4kb页数量
    uint64_t lowWatermarkPages;         // 本地低水线，4kb页数量
    std::vector<std::string> dockerIds; // 迁移目标容器ids
    std::vector<int32_t> srcNids;       // 本地迁出节点
};

class MigrationExecutor {
public:
    static MigrationExecutor &GetInstance()
    {
        static MigrationExecutor instance;
        return instance;
    }

    uint32_t ExecuteNewMigrationStrategy(const MigrationStrategy &newStrategy); // 内存迁移策略执行

    MigrationExecutor(const MigrationExecutor &) = delete;
    MigrationExecutor &operator=(const MigrationExecutor &) = delete;

private:
    ~MigrationExecutor();
    MigrationExecutor() = default;

    std::vector<pid_t> pidsToMigrate_;

    // 迁移速率控制，单位ms
    uint32_t migrateInterval_{1000};

    std::optional<MigrationStrategy> currentStrategy_;
    std::mutex actionMutex_;
    std::thread migrationWorkerThread_;
    std::atomic<bool> shouldRun_{false};
    std::atomic<bool> alreadyStarting_{false};

    void StartMigrationWorker();
    void StopMigrationWorker();
    void MigrationWorkerTask();
    bool IsHighWatermarkExceeded(const MigrationStrategy &strategy);
    bool IsLowWatermarkExceeded(const MigrationStrategy &strategy);
    void PerformMigration(const MigrationStrategy &strategy);

    pid_t GetDockerPid(const std::string &dockerId);
    uint32_t RecordPids(const MigrationStrategy &strategy);
    uint64_t GetLocalMemFreeKB(const MigrationStrategy &strategy);
    void StopCurrentMigrationStrategy();
};
} // namespace turbo::ucache
#endif /* MIGRATION_EXECUTOR_H */