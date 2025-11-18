/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */#ifndef UCACHE_MIGRATE_EXCUTOR_H
#define UCACHE_MIGRATE_EXCUTOR_H

#include <vector>
#include <optional>
#include <thread>
#include <mutex>
#include <atomic>

namespace ucache {

namespace migrate_excutor {

struct MigrationStrategy {
    std::vector<uint16_t> desNids;       // 迁出目标节点（远端内存）
    int16_t srcNid;                     // 本地迁出节点(如果小于0，表示所有节点)
    float ucacheUsageRatio;             // 迁移水位
    std::vector<pid_t> pids;          // 需要迁移的进程列表
};

class UcacheMigrationExecutor {
public:
    static UcacheMigrationExecutor &GetInstance()
    {
        static UcacheMigrationExecutor instance;
        return instance;
    }

    uint32_t ExecuteNewMigrationStrategy(const MigrationStrategy &newStrategy); // 内存迁移策略执行
    uint32_t StopCurrentMigrationStrategy();                                    // 停止当前内存迁移策略

    UcacheMigrationExecutor(const UcacheMigrationExecutor &) = delete;
    UcacheMigrationExecutor &operator=(const UcacheMigrationExecutor &) = delete;

private:
    ~UcacheMigrationExecutor();
    UcacheMigrationExecutor() = default;

    std::vector<pid_t> pidsToMigrate_;

    // 迁移速率控制，单位ms
    uint32_t migrateInterval_{500};
    float targetUsageRatio_{0.0};

    std::optional<MigrationStrategy> currentStrategy_;
    std::mutex actionMutex_;
    std::thread migrationWorkerThread_;
    std::atomic<bool> shouldRun_{false};
    std::atomic<bool> alreadyStarting_{false};

    void StartMigrationWorker();
    void StopMigrationWorker();
    void MigrationWorkerTask();
    void PerformMigration(const MigrationStrategy &strategy);

    // 执行迁移任务
    uint32_t ExcuteMigrateFolios(const uint16_t desNid, const int16_t srcNid, const pid_t pid, uint32_t &success);

    // 向单个远端numa节点节点迁移内存页
    void MigrateToRemoteNode(const uint16_t desNid, const MigrationStrategy &strategy);

    // 迁移指定本地numa内存到远端
    uint32_t MigrateFromSingleSrcNid(const uint16_t desNid, const int16_t srcNid, const pid_t pid);

    // 迁移所有本地numa内存到远端
    uint32_t MigrateFromMultipleSrcNids(const uint16_t desNid, const pid_t pid);

    // 是否远端节点都满足水线
    bool IsRemoteWatermarkExceeded(const MigrationStrategy &strategy);
    // 该节点是否满足水线
    bool IsNidWatermarkExceeded(const uint16_t nid);
    // 获取本地numa节点的nid列表
    uint32_t GetLocalNumaIDs(std::vector<int16_t> &nids);
    // 根据pid获取其所在containerd的id
    std::string GetContainerdIdByPid(const pid_t pid);
    // 记录需要迁移的pid列表
    uint32_t RecordPids(const MigrationStrategy &strategy);
    // 根据nid查询FilePage大小
    uint64_t GetMemFilePagesForNode(uint16_t nid);
    // 根据pid查询MemTotal大小
    uint64_t GetMemTotalForNode(uint16_t nid);
};

}


}   // namespace ucache

#endif