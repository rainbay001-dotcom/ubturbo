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

#include "task_executor.h"

#include <securec.h>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include "resource_collector.h"
#include "turbo_ipc_server.h"
#include "turbo_serialize.h"
#include "turbo_strategy_executor.h"
#include "ucache_turbo_config.h"
#include "ucache_turbo_error.h"

namespace turbo::ucache {
using namespace turbo::ipc::server;
using namespace turbo::log;
using namespace turbo::serialize;

uint32_t TaskExecutor::Init()
{
    UBTurboRegIpcService("UCacheExecuteTask", TaskExecutor::UCacheExecuteTask);
    return UCACHE_OK;
}

uint64_t GetCurrentTimestamp()
{
    return static_cast<uint64_t>(std::time(nullptr));
}

uint32_t HandleCollectResource(const TaskRequest &tReq, TaskResponse &tResp)
{
    ResourceQueryType qType;
    {
        UCacheInStream in(tReq.payload);
        in >> qType;
    }
    uint32_t ret = UCACHE_OK;
    UCacheOutStream out{};
    UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Start to collect resource, resourceType=" << ResourceQueryTypeToString(qType) << ".";
    switch (qType) {
        case ResourceQueryType::NUMA_INFO: {
            std::unordered_map<std::string, NodeInfo> numaInfos{};
            ret = GetNumaInfo(numaInfos);
            for (auto &numaInfo : numaInfos) {
                UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                    << "node=" << numaInfo.first << "info=" << numaInfo.second.ToString() << ".";
            }
            out << numaInfos;
            break;
        }
        case ResourceQueryType::CGROUP_INFO: {
            std::unordered_map<std::string, CgroupInfos> cgroupInfos{};
            ret = GetCgroupsInfo(cgroupInfos);
            for (auto &cgroupInfo : cgroupInfos) {
                UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                    << "cgroup=" << cgroupInfo.first << "info=" << cgroupInfo.second.ToString() << ".";
            }
            out << cgroupInfos;
            break;
        }
        case ResourceQueryType::MEM_WATERMARK: {
            MemWatermarkInfo memWatermark{};
            ret = GetMemWatermarkInfo(memWatermark);
            UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "MinFreeKBytes=" << memWatermark.minFreeKBytes << ".";
            out << memWatermark;
            break;
        }
    }
    tResp.resCode = ret;
    if (ret != UCACHE_OK) {
        return ret;
    }
    tResp.payload = out.Str();
    tResp.timestamp = GetCurrentTimestamp();
    return ret;
}

MigrationStrategy ConvertToMigrationStrategy(const MigrationStrategyParam &param)
{
    MigrationStrategy strategy;
    strategy.dstNid = param.dstNid;
    strategy.highWatermarkPages = param.highWatermarkPages;
    strategy.lowWatermarkPages = param.lowWatermarkPages;
    strategy.dockerIds = param.dockerIds;
    strategy.srcNids = param.srcNids;
    return strategy;
}

uint32_t HandleMigrationStrategy(const TaskRequest &tReq, TaskResponse &tResp)
{
    MigrationStrategyParam param;
    {
        UCacheInStream in(tReq.payload);
        in >> param;
    }
    UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << param.ToString() << ".";
    MigrationStrategy strategy = ConvertToMigrationStrategy(param);
    UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start to execute migration strategy.";
    uint32_t ret = TurboStrategyExecutor::ExecuteMigrationStrategy(strategy);
    if (ret != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "ExecuteMigrationStrategy failed, ret=" << ret;
        return ret;
    }
    return UCACHE_OK;
}

uint32_t ValidateCollectResourceParam(const std::string &payload)
{
    ResourceQueryType qType;
    {
        UCacheInStream in(payload);
        in >> qType;
    }
    if (!IsValidResourceQueryType(qType)) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Unknown resourceType, resourceType=" << ResourceQueryTypeToString(qType) << ".";
        return EXECUTE_INVALID_PARAM;
    }
    return UCACHE_OK;
}

bool ValidateStrategyParam(const MigrationStrategyParam &param)
{
    if (param.srcNids.empty() || param.dockerIds.empty()) {
        return false;
    }
    for (int nid : param.srcNids) {
        if (nid < 0) {
            return false;
        }
    }
    if (param.dstNid < 0) {
        return false;
    }
    if (param.highWatermarkPages < param.lowWatermarkPages) {
        return false;
    }
    return true;
}

uint32_t ValidateMigrationStrategyParams(const std::string &payload)
{
    MigrationStrategyParam param;
    {
        UCacheInStream in(payload);
        in >> param;
    }
    if (!ValidateStrategyParam(param)) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid strategy param.";
        UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << param.ToString() << ".";
        return EXECUTE_INVALID_PARAM;
    }
    return UCACHE_OK;
}

uint32_t ValidateTaskParams(const TaskRequest &tReq)
{
    if (!IsValidTaskType(tReq.type)) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Invalid taskType, taskType=" << TaskTypeToString(tReq.type) << ".";
        return INVALID_TASK_TYPE;
    }
    switch (tReq.type) {
        case TaskType::COLLECT_RESOURCE:
            return ValidateCollectResourceParam(tReq.payload);
        case TaskType::MIGRATION_STRATEGY:
            return ValidateMigrationStrategyParams(tReq.payload);
        default:
            return INVALID_TASK_TYPE;
    }
    return INVALID_TASK_TYPE;
}

uint32_t TaskExecutor::UCacheExecuteTask(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
    if (!req.data && req.len > 0) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid byteBuffer.";
        return INVALID_BUFFER;
    }
    if (req.len == 0) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Empty byteBuffer.";
        return EMPTY_BUFFER;
    }
    TaskRequest tReq{};
    {
        UCacheInStream in(req.data, req.len);
        in >> tReq;
    }
    TaskResponse tResp{};
    uint32_t ret = UCACHE_OK;
    ret = ValidateTaskParams(tReq);
    if (ret != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Validate task params failed.";
        tResp.resCode = ret;
    } else {
        switch (tReq.type) {
            case TaskType::COLLECT_RESOURCE:
                ret = HandleCollectResource(tReq, tResp);
                break;
            case TaskType::MIGRATION_STRATEGY:
                ret = HandleMigrationStrategy(tReq, tResp);
                break;
            default:
                return INVALID_TASK_TYPE;
        }
    }
    if (ret != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Execute task failed, ret=" << ret;
    }
    UCacheOutStream out;
    out << tResp;
    resp.len = out.GetSize();
    resp.data = out.GetBufferPointer();
    resp.freeFunc = [](uint8_t *d) {
        delete[] d;
    };
    return UCACHE_OK;
}

} // namespace turbo::ucache