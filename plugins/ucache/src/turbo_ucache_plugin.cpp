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

#include <cstdint>
#include <string>
#include "task_executor.h"
#include "turbo_logger.h"
#include "ucache_turbo_config.h"
#include "ucache_turbo_error.h"
#include "driver_interaction.h"

using namespace turbo::log;
using namespace turbo::ucache;

extern "C" {
uint32_t TurboPluginInit(const uint16_t modCode);
void TurboPluginDeInit();
}

uint32_t TurboPluginInit(const uint16_t modCode)
{
    UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start init os_turbo_plugin, modCode: " << modCode;
    auto ret = UcacheTurboConfig::GetInstance().Initialize(modCode);
    if (ret != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to init ucache config, ret: " << ret;
        return ret;
    }
    ret = TaskExecutor::Init();
    if (ret != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to init TaskExecutor, ret: " << ret;
        return ret;
    }
    UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Init os_turbo_plugin successfully, modCode: " << modCode;
    return UCACHE_OK;
}

void TurboPluginDeInit()
{
    UBTURBO_LOG_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start deinit plugin";
}