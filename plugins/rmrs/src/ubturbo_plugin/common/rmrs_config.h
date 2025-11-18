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
#ifndef RMRS_CONFIGURATION_H
#define RMRS_CONFIGURATION_H

#include <string>
#include "rmrs_error.h"

namespace rmrs {

#define RMRS_MODULE_NAME RmrsConfig::Instance().GetModuleName()
#define RMRS_MODULE_CODE RmrsConfig::Instance().GetModuleCode()
#define RMRS_CONFIG_NAME RmrsConfig::Instance().GetConfigName()

class RmrsConfig {
public:
    static RmrsConfig &Instance()
    {
        static RmrsConfig instance;
        return instance;
    }

    RmrsConfig(const RmrsConfig &) = delete;
    RmrsConfig &operator=(const RmrsConfig &) = delete;
    RmrsConfig(RmrsConfig &&) = delete;
    RmrsConfig &operator=(RmrsConfig &&) = delete;

    inline const char *GetModuleName()
    {
        return moduleName.c_str();
    }

    inline const char *GetConfigName()
    {
        return configName.c_str();
    }

    inline uint16_t GetModuleCode()
    {
        return moduleCode;
    }

    bool GetRmrsUcacheEnable();

    inline RmrsResult Init(const uint16_t modCode)
    {
        moduleCode = modCode;
        RmrsLoadConfig();
        return RMRS_OK;
    }

private:
    RmrsConfig() = default;
    void RmrsLoadConfig();

    std::string moduleName = "rmrs";
    uint16_t moduleCode = 0;
    std::string configName = "plugin_rmrs";
    bool rmrsUCacheEnable = false;
};
} // namespace rmrs

#endif
