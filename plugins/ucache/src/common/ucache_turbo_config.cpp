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

#include "ucache_turbo_config.h"

#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

namespace turbo::ucache {
using namespace turbo::config;
using namespace turbo::log;

uint32_t UcacheTurboConfig::Initialize(const uint16_t modCode)
{
    moduleCode = modCode;
    return LoadConfig();
}

uint32_t UcacheTurboConfig::LoadMigrateConfig()
{
    uint32_t ret = UCACHE_OK;
    if ((ret = GetConfigValue(UBTurboGetUInt32, "plugin_turbo_ucache", "migrate.migrateInterval", migrateInterval)) !=
        UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) <<
            "Invalid config migrateInterval, value is" << migrateInterval;
        return ret;
    }
    if (migrateInterval <= 0) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) <<
            "migrateInterval is invalid, migrateInterval: " << migrateInterval;
        return UCACHE_ERR;
    }
    return ret;
}

uint32_t UcacheTurboConfig::LoadConfig()
{
    uint32_t ret = UCACHE_OK;
    if ((ret = LoadMigrateConfig()) != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "LoadConfig migrateInterval failed";
        return ret;
    }
    return UCACHE_OK;
}

uint32_t UcacheTurboConfig::GetMigrateInterval() const
{
    return migrateInterval;
}
} // namespace turbo::ucache