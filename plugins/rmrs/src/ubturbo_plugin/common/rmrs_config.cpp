/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "rmrs_config.h"
 
#include <cstdint>
#include "turbo_conf.h"
#include "turbo_logger.h"
 
namespace rmrs {
using namespace turbo::config;
using namespace turbo::log;
 
 
void RmrsConfig::RmrsLoadConfig()
{
    uint32_t ret = UBTurboGetBool("plugin_rmrs", "rmrs.ucache.enable", rmrsUCacheEnable);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "Get config value failed, key=rmrs.ucache.enable, ret=" << ret << ", using default value false.";
        rmrsUCacheEnable = false;
    }
}
 
bool RmrsConfig::GetRmrsUcacheEnable()
{
    return rmrsUCacheEnable;
}
 
} // namespace ucache_turbo