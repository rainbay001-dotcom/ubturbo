/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */#include "rmrs_smap_module.h"
#include <sys/stat.h>
#include <unistd.h>
#include "rmrs_config.h"
#include "turbo_conf.h"
#include "turbo_logger.h"

namespace rmrs::smap {
using namespace turbo::log;

void *SmapModule::smapHandle = nullptr;
SmapInitFunc SmapModule::smapInitFunc = nullptr;
SmapMigrateOutFunc SmapModule::smapMigrateOutFunc = nullptr;
SmapMigrateOutSyncFunc SmapModule::smapMigrateOutSyncFunc = nullptr;
SetSmapRemoteNumaInfoFunc SmapModule::setSmapRemoteNumaInfoFunc = nullptr;
SetSmapRunModeFunc SmapModule::setSmapRunModeFunc = nullptr;
SmapRemoveFunc SmapModule::smapRemoveFunc = nullptr;
SmapEnableNodeFunc SmapModule::smapEnableNodeFunc = nullptr;
SmapMigratePidRemoteNumaFunc SmapModule::smapMigratePidRemoteNumaFunc = nullptr;
SmapEnableProcessMigrateFunc SmapModule::smapEnableProcessMigrateFunc = nullptr;
SmapQueryProcessConfigFunc SmapModule::smapQueryProcessConfigFunc = nullptr;

RmrsResult SmapModule::Init()
{
    struct stat sb;
    if (lstat(SMAP_LIBSMAPSO_PATH_RMRS, &sb) == 0 && S_ISLNK(sb.st_mode)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] SMAP path is a symbolic link.";
        return RMRS_ERROR;
    }
    smapHandle = dlopen(SMAP_LIBSMAPSO_PATH_RMRS, RTLD_LAZY);
    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Load libsmap.so failed.";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

SmapInitFunc SmapModule::GetSmapInit()
{
    if (smapInitFunc != nullptr) {
        return smapInitFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapInitFunc = (SmapInitFunc)(dlsym(smapHandle, "ubturbo_smap_start"));
    if (smapInitFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Get ubturbo_smap_start ptr failed.";
        return nullptr;
    }
    return smapInitFunc;
}

SetSmapRemoteNumaInfoFunc SmapModule::GetSetSmapRemoteNumaInfo()
{
    if (setSmapRemoteNumaInfoFunc != nullptr) {
        return setSmapRemoteNumaInfoFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    setSmapRemoteNumaInfoFunc = (SetSmapRemoteNumaInfoFunc)(dlsym(smapHandle, "ubturbo_smap_remote_numa_info_set"));
    if (setSmapRemoteNumaInfoFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_remote_numa_info_set ptr failed.";
        return nullptr;
    }
    return setSmapRemoteNumaInfoFunc;
}

SmapMigrateOutFunc SmapModule::GetSmapMigrateOut()
{
    if (smapMigrateOutFunc != nullptr) {
        return smapMigrateOutFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapMigrateOutFunc = (SmapMigrateOutFunc)(dlsym(smapHandle, "ubturbo_smap_migrate_out"));
    if (smapMigrateOutFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_migrate_out ptr failed.";
        return nullptr;
    }
    return smapMigrateOutFunc;
}

SmapMigrateOutSyncFunc SmapModule::GetSmapMigrateOutSync()
{
    if (smapMigrateOutSyncFunc != nullptr) {
        return smapMigrateOutSyncFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapMigrateOutSyncFunc = (SmapMigrateOutSyncFunc)(dlsym(smapHandle, "ubturbo_smap_migrate_out_sync"));
    if (smapMigrateOutSyncFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_migrate_out_sync ptr failed.";
        return nullptr;
    }
    return smapMigrateOutSyncFunc;
}

void SmapModule::CloseSmapHandle()
{
    if (smapHandle) {
        if (dlclose(smapHandle) != 0) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsSmapModule] Close SmapHandle failed:" << dlerror() << ".";
        }
        smapHandle = nullptr; // 关闭后重置，防止重复关闭
    }
    smapInitFunc = nullptr;
    smapMigrateOutFunc = nullptr;
    smapHandle = nullptr;
    smapMigrateOutSyncFunc = nullptr;
    setSmapRemoteNumaInfoFunc = nullptr;
    setSmapRunModeFunc = nullptr;
    smapRemoveFunc = nullptr;
    smapEnableNodeFunc = nullptr;
    smapMigratePidRemoteNumaFunc = nullptr;
    smapEnableProcessMigrateFunc = nullptr;
    smapQueryProcessConfigFunc = nullptr;
}

SetSmapRunModeFunc SmapModule::GetSetSmapRunModeFunc()
{
    if (setSmapRunModeFunc != nullptr) {
        return setSmapRunModeFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    setSmapRunModeFunc = (SetSmapRunModeFunc)(dlsym(smapHandle, "ubturbo_smap_run_mode_set"));
    if (setSmapRunModeFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_run_mode_set ptr failed.";
        return nullptr;
    }
    return setSmapRunModeFunc;
}

SmapRemoveFunc SmapModule::GetSmapRemoveFunc()
{
    if (smapRemoveFunc != nullptr) {
        return smapRemoveFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapRemoveFunc = (SmapRemoveFunc)(dlsym(smapHandle, "ubturbo_smap_remove"));
    if (smapRemoveFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Get ubturbo_smap_remove ptr failed.";
        return nullptr;
    }
    return smapRemoveFunc;
}

SmapEnableNodeFunc SmapModule::GetSmapEnableNodeFunc()
{
    if (smapEnableNodeFunc != nullptr) {
        return smapEnableNodeFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapEnableNodeFunc = (SmapEnableNodeFunc)(dlsym(smapHandle, "ubturbo_smap_node_enable"));
    if (smapEnableNodeFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_node_enable ptr failed.";
        return nullptr;
    }
    return smapEnableNodeFunc;
}

SmapMigratePidRemoteNumaFunc SmapModule::GetSmapMigratePidRemoteNumaFunc()
{
    if (smapMigratePidRemoteNumaFunc != nullptr) {
        return smapMigratePidRemoteNumaFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapMigratePidRemoteNumaFunc =
        (SmapMigratePidRemoteNumaFunc)(dlsym(smapHandle, "ubturbo_smap_pid_remote_numa_migrate"));
    if (smapMigratePidRemoteNumaFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_pid_remote_numa_migrate ptr failed.";
        return nullptr;
    }
    return smapMigratePidRemoteNumaFunc;
}

SmapEnableProcessMigrateFunc SmapModule::GetSmapEnableProcessMigrateFunc()
{
    if (smapEnableProcessMigrateFunc != nullptr) {
        return smapEnableProcessMigrateFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapEnableProcessMigrateFunc = (SmapEnableProcessMigrateFunc)dlsym(smapHandle, "SmapEnableProcessMigrate");
    if (smapEnableProcessMigrateFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapModule] Get SmapEnableProcessMigrate ptr failed.";
        return nullptr;
    }
    return smapEnableProcessMigrateFunc;
}

SmapQueryProcessConfigFunc SmapModule::GetSmapQueryProcessConfigFunc()
{
    if (smapQueryProcessConfigFunc != nullptr) {
        return smapQueryProcessConfigFunc;
    }

    if (smapHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapQueryProcessConfigFunc = (SmapQueryProcessConfigFunc)dlsym(smapHandle, "ubturbo_smap_process_config_query");
    if (smapQueryProcessConfigFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_process_config_query ptr failed.";
        return nullptr;
    }
    return smapQueryProcessConfigFunc;
}

} // namespace rmrs::smap
