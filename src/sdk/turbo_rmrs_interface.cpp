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

#include "turbo_rmrs_interface.h"
#include "turbo_serialize.h"
#include "turbo_ipc_client.h"
#include "ulog.h"
#include "turbo_error.h"

namespace turbo::rmrs {

using namespace turbo::serialize;
using namespace turbo::ipc::client;

uint32_t UBTurboRMRSAgentMigrateExecute(const MigrateStrategyResult &migrateStrategyResult)
{
    RmrsOutStream builder;
    builder << migrateStrategyResult;
    TurboByteBuffer params;
    params.len = builder.GetSize();
    params.data = builder.GetBufferPointer();
    
    TurboByteBuffer result;
    uint32_t ret = UBTurboFunctionCaller("MigrateExecuteRecvHandler", params, result);
    delete[] params.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[MemMigrate][MemMigrate] UBTurboFunctionCaller failed.");
        return ret;
    }
    if (result.len != 1 || result.data[0] != 0) {
        IPC_CLIENT_LOGGER_ERROR("[MemMigrate][MemMigrate] MigrateExecute error.");
        delete[] result.data;
        return TURBO_ERROR;
    }

    delete[] result.data;

    IPC_CLIENT_LOGGER_DEBUG("[MemMigrate][MemMigrate] MigrateExecuteImpl by ipc end.");
    return TURBO_OK;
}

uint32_t UBTurboRMRSAgentMigrateStrategy(const MigrateStrategyParamRMRS &migrateStrategyParam,
                                         MigrateStrategyResult &migrateStrategyResult)
{
    IPC_CLIENT_LOGGER_DEBUG("[MemMigrate][Strategy] IPC message sent started.");

    RmrsOutStream builder;
    builder << migrateStrategyParam;

    TurboByteBuffer reqTurbo;
    TurboByteBuffer respTurbo;

    reqTurbo.data = builder.GetBufferPointer();
    reqTurbo.len = builder.GetSize();

    auto ret = UBTurboFunctionCaller("MigrateStrategyRecvHandler", reqTurbo, respTurbo);
    delete[] reqTurbo.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[MemMigrate][Strategy] MigrateStrategyRecvHandler failed res: %u.", ret);
        return ret;
    }

    RmrsInStream builderOut(respTurbo.data, respTurbo.len);
    builderOut >> migrateStrategyResult;

    delete[] respTurbo.data;

    IPC_CLIENT_LOGGER_DEBUG("[MemMigrate][Strategy] IPC message sent successfully.");
    return TURBO_OK;
}

uint32_t UBTurboRMRSAgentMigrateBack(MigrateBackResult &migrateBackResult)
{
    IPC_CLIENT_LOGGER_DEBUG("[MemMigrate][Back] IPC message sent started.");

    TurboByteBuffer reqTurbo;
    TurboByteBuffer respTurbo;

    reqTurbo.data = new uint8_t[1];
    reqTurbo.len = sizeof(uint8_t);

    auto ret = UBTurboFunctionCaller("MigrateBackRecvHandler", reqTurbo, respTurbo);
    delete[] reqTurbo.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[MemMigrate][Back] MigrateBackRecvHandler failed res: %u.", ret);
        return ret;
    }

    RmrsInStream builderOut(respTurbo.data, respTurbo.len);
    builderOut >> migrateBackResult;

    delete[] respTurbo.data;

    IPC_CLIENT_LOGGER_DEBUG("[MemMigrate][Back] IPC message sent successfully.");
    return TURBO_OK;
}

uint32_t UBTurboRMRSAgentBorrowRollBack(std::map<std::string, std::set<BorrowIdInfo>> &borrowIdsPidsMap)
{
    IPC_CLIENT_LOGGER_DEBUG("[MemRollback] IPC message sent started.");

    RmrsOutStream builder;
    builder << borrowIdsPidsMap;

    TurboByteBuffer reqTurbo;
    TurboByteBuffer respTurbo;

    reqTurbo.data = builder.GetBufferPointer();
    reqTurbo.len = builder.GetSize();

    auto ret = UBTurboFunctionCaller("BorrowRollbackRecvHandler", reqTurbo, respTurbo);
    delete[] reqTurbo.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[MemRollback] BorrowRollbackRecvHandler failed res: %u.", ret);
        return ret;
    }

    if (respTurbo.data == nullptr || respTurbo.len == 0 || respTurbo.data[0] != 0) {
        IPC_CLIENT_LOGGER_ERROR("[MemRollback] The result of UBTurboFunctionCaller is error.");
        delete[] respTurbo.data;
        return TURBO_ERROR;
    }

    delete[] respTurbo.data;

    IPC_CLIENT_LOGGER_DEBUG("[MemRollback] IPC message sent successfully.");
    return TURBO_OK;
}

uint32_t UBTurboRMRSAgentPidNumaInfoCollect(const PidNumaInfoCollectParam &pidNumaInfoCollectParam,
                                            PidNumaInfoCollectResult &pidNumaInfoCollectResult)
{
    IPC_CLIENT_LOGGER_DEBUG("[PidNumaInfoCollect] IPC message sent started.");

    RmrsOutStream builder;
    builder << pidNumaInfoCollectParam;

    TurboByteBuffer reqTurbo;
    TurboByteBuffer respTurbo;

    reqTurbo.data = builder.GetBufferPointer();
    reqTurbo.len = builder.GetSize();

    auto ret = UBTurboFunctionCaller("PidNumaInfoCollectRecvHandler", reqTurbo, respTurbo);
    delete[] reqTurbo.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[PidNumaInfoCollect] PidNumaInfoCollectRecvHandler failed res: %u.", ret);
        return ret;
    }

    RmrsInStream builderOut(respTurbo.data, respTurbo.len);
    builderOut >> pidNumaInfoCollectResult;
    delete[] respTurbo.data;

    IPC_CLIENT_LOGGER_DEBUG("[PidNumaInfoCollect] IPC message sent successfully.");
    return TURBO_OK;
}

uint32_t UBTurboRMRSAgentNumaMemInfoCollect(const NumaMemInfoCollectParam &numaMemInfoCollectParam,
                                            ResponseInfoSimpo &responseInfoSimpo)
{
    IPC_CLIENT_LOGGER_DEBUG("[NumaMemInfoCollect] IPC message sent started.");

    RmrsOutStream builder;
    builder << numaMemInfoCollectParam;

    TurboByteBuffer reqTurbo;
    TurboByteBuffer respTurbo;

    reqTurbo.data = builder.GetBufferPointer();
    reqTurbo.len = builder.GetSize();

    auto ret = UBTurboFunctionCaller("NumaMemInfoCollectRecvHandler", reqTurbo, respTurbo);
    delete[] reqTurbo.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[NumaMemInfoCollect] NumaMemInfoCollectRecvHandler failed res: %u.", ret);
        return ret;
    }

    RmrsInStream builderOut(respTurbo.data, respTurbo.len);
    builderOut >> responseInfoSimpo;
    delete[] respTurbo.data;

    IPC_CLIENT_LOGGER_DEBUG("[NumaMemInfoCollect] IPC message sent successfully.");
    return TURBO_OK;
}

uint32_t UBTurboRMRSAgentUCacheMigrateStrategy(const UCacheMigrationStrategyParam &uCacheMigrationStrategyParam,
                                               ResCode &resCode)
{
    IPC_CLIENT_LOGGER_DEBUG("[UCache] Send UBTurboRMRSAgentUCacheMigrateStrategy ipc message started.");
    RmrsOutStream builder;
    builder << uCacheMigrationStrategyParam;
 
    TurboByteBuffer reqTurbo;
    TurboByteBuffer respTurbo;
    reqTurbo.data = builder.GetBufferPointer();
    reqTurbo.len = builder.GetSize();
 
    auto ret = UBTurboFunctionCaller("UCacheMigrateStrategyRecvHandler", reqTurbo, respTurbo);
    delete[] reqTurbo.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[UCache] Send UCacheMigrateStrategyRecvHandler failed res: %u.", ret);
        return ret;
    }
 
    RmrsInStream builderOut(respTurbo.data, respTurbo.len);
    builderOut >> resCode;
    delete[] respTurbo.data;
    IPC_CLIENT_LOGGER_DEBUG("[UCache] UBTurboRMRSAgentUCacheMigrateStrategy ipc message sent successfully.");
    return TURBO_OK;
}

uint32_t UBTurboRMRSAgentUCacheMigrateStop(ResCode &resCode)
{
    IPC_CLIENT_LOGGER_DEBUG("[UCache] Send UBTurboRMRSAgentUCacheMigrateStop ipc message started.");
    ResCode blankParam{}; // 实际无需入参，仅用于生成TurboByteBuffer结构体
    RmrsOutStream builder;
    builder << blankParam;
 
    TurboByteBuffer reqTurbo;
    TurboByteBuffer respTurbo;
    reqTurbo.data = builder.GetBufferPointer();
    reqTurbo.len = builder.GetSize();
 
    auto ret = UBTurboFunctionCaller("UCacheMigrateStopRecvHandler", reqTurbo, respTurbo);
    delete[] reqTurbo.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[UCache] Send UCacheMigrateStopRecvHandler failed res: %u.", ret);
        return ret;
    }
 
    RmrsInStream builderOut(respTurbo.data, respTurbo.len);
    builderOut >> resCode;
    delete[] respTurbo.data;
    IPC_CLIENT_LOGGER_DEBUG("[UCache] UBTurboRMRSAgentUCacheMigrateStop ipc message sent successfully.");
    return TURBO_OK;
}
 
uint32_t UBTurboRMRSAgentUpdateUCacheRatio(const MigrationInfoParam &migrationInfoParam, UCacheRatioRes &uCacheRatioRes)
{
    IPC_CLIENT_LOGGER_DEBUG("[UCache] Send UBTurboRMRSAgentUpdateUCacheRatio ipc message started.");
    RmrsOutStream builder;
    builder << migrationInfoParam;
 
    TurboByteBuffer reqTurbo;
    TurboByteBuffer respTurbo;
    reqTurbo.data = builder.GetBufferPointer();
    reqTurbo.len = builder.GetSize();
 
    auto ret = UBTurboFunctionCaller("UpdateUCacheRatioRecvHandler", reqTurbo, respTurbo);
    delete[] reqTurbo.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[UCache] Send UpdateUCacheRatioRecvHandler failed res: %u.", ret);
        return ret;
    }
 
    RmrsInStream builderOut(respTurbo.data, respTurbo.len);
    builderOut >> uCacheRatioRes;
    delete[] respTurbo.data;
    IPC_CLIENT_LOGGER_DEBUG("[UCache] UBTurboRMRSAgentUpdateUCacheRatio ipc message sent successfully.");
    return TURBO_OK;
}
} // namespace turbo::rmrs