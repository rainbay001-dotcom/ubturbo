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

#ifndef IPC_RECV_HANDLER_H
#define IPC_RECV_HANDLER_H

#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "securec.h"

#include "turbo_ipc_server.h"
#include "ucache_turbo_error.h"

namespace turbo::ucache {
using namespace turbo::ipc::server;

struct MigrationStrategyParam {
    int32_t dstNid{};
    uint64_t highWatermarkPages{};
    uint64_t lowWatermarkPages{};
    std::vector<std::string> dockerIds{};
    std::vector<int32_t> srcNids{};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "dstNid:" << dstNid << ",";
        oss << "highWatermarkPages:" << highWatermarkPages << ",";
        oss << "lowWatermarkPages:" << lowWatermarkPages << ",";
        oss << "dockerIds:[";
        for (size_t i = 0; i < dockerIds.size(); ++i) {
            oss << "\"" << dockerIds[i] << "\"";
            if (i != dockerIds.size() - 1) {
                oss << ", ";
            }
        }
        oss << "],";
        oss << "srcNids:[";
        for (size_t i = 0; i < srcNids.size(); ++i) {
            oss << srcNids[i];
            if (i != srcNids.size() - 1) {
                oss << ", ";
            }
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

enum class TaskType : uint32_t {
    INVALID = 0xFFFFFFFF,
    COLLECT_RESOURCE = 0,
    MIGRATION_STRATEGY = 1,
};

inline const char *TaskTypeToString(TaskType t)
{
    switch (t) {
        case TaskType::COLLECT_RESOURCE:
            return "CollectResource";
        case TaskType::MIGRATION_STRATEGY:
            return "MigrationStrategy";
        case TaskType::INVALID:
            return "InvalidTaskType";
    }
    return "UnknownTaskType";
}

struct TaskRequest {
    TaskType type = TaskType::INVALID;
    std::string payload{};
};

inline bool IsValidTaskType(const TaskType t)
{
    switch (t) {
        case TaskType::COLLECT_RESOURCE:
        case TaskType::MIGRATION_STRATEGY:
            return true;
        case TaskType::INVALID:
            return false;
    }
    return false;
}

enum class ResourceQueryType : uint32_t {
    NUMA_INFO = 0,
    CGROUP_INFO = 1,
    MEM_WATERMARK = 2,
};

inline const char *ResourceQueryTypeToString(ResourceQueryType t)
{
    switch (t) {
        case ResourceQueryType::NUMA_INFO:
            return "NumaInfo";
        case ResourceQueryType::CGROUP_INFO:
            return "CgroupInfo";
        case ResourceQueryType::MEM_WATERMARK:
            return "MemWatermark";
    }
    return "UnknownResourceQueryType";
}

inline bool IsValidResourceQueryType(const ResourceQueryType t)
{
    switch (t) {
        case ResourceQueryType::NUMA_INFO:
            return true;
        case ResourceQueryType::CGROUP_INFO:
            return true;
        case ResourceQueryType::MEM_WATERMARK:
            return true;
    }
    return false;
}

struct TaskResponse {
    uint32_t resCode = EXECUTE_DEFAULT_ERROR;
    uint64_t timestamp{};
    std::string payload{};
};

class TaskExecutor {
public:
    static uint32_t Init();
    // 该接口只返回0值，否则框架不会传输resp数据，真实返回值通过封装的结构体TaskResponse::resCode表示
    static uint32_t UCacheExecuteTask(const TurboByteBuffer &req, TurboByteBuffer &resp);
};
} // namespace turbo::ucache

#endif /* IPC_RECV_HANDLER_H */