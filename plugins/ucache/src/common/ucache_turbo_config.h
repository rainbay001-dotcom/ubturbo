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

#ifndef UCACHE_TURBO_CONFIG_H
#define UCACHE_TURBO_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

#include "turbo_conf.h"
#include "turbo_logger.h"
#include "ucache_turbo_error.h"

namespace turbo::ucache {
inline constexpr auto UCACHE_MODULE_NAME = "ucache_plugin";
#define UCACHE_MODULE_CODE UcacheTurboConfig::GetInstance().GetModuleCode()

class UcacheTurboConfig {
public:
    static UcacheTurboConfig &GetInstance()
    {
        static UcacheTurboConfig instance;
        return instance;
    }
    uint32_t Initialize(const uint16_t modCode);
    inline uint16_t GetModuleCode()
    {
        return moduleCode;
    }
    uint32_t GetMigrateInterval() const;

    UcacheTurboConfig(const UcacheTurboConfig &) = delete;
    UcacheTurboConfig &operator=(const UcacheTurboConfig &) = delete;

private:
    UcacheTurboConfig() = default;
    ~UcacheTurboConfig() = default;
    uint32_t LoadConfig();
    uint32_t LoadMigrateConfig();

    template <typename Func, typename ValueType>
    uint32_t GetConfigValue(Func &&func, const std::string &section, const std::string &key, ValueType &value)
    {
        uint32_t ret = func(section, key, value);
        if (ret != UCACHE_OK) {
            UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) <<
                "get config value failed, section: " << section << ", key: " << key << ", ret: " << ret;
            return ret;
        }
        return UCACHE_OK;
    }
    // 默认迁移间隔1000ms
    uint32_t migrateInterval{1000};
    uint16_t moduleCode = 0;
};
} // namespace turbo::ucache

#endif // UCACHE_TURBO_CONFIG_H