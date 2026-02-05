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

#include "turbo_strategy_executor.h"
#include "ucache_turbo_config.h"
#include "ucache_turbo_error.h"

using namespace turbo::log;

namespace turbo::ucache {

uint32_t TurboStrategyExecutor::ExecuteMigrationStrategy(MigrationStrategy &strategy)
{
    uint32_t ret = MigrationExecutor::GetInstance().ExecuteNewMigrationStrategy(strategy);
    if (ret != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to execute migration strategy.";
        return UCACHE_ERR;
    }
    return UCACHE_OK;
}
} // namespace turbo::ucache