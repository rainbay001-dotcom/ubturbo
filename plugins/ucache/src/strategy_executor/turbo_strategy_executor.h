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

#ifndef TURBO_STRATEGY_EXECUTOR_H
#define TURBO_STRATEGY_EXECUTOR_H

#include <cstdint>
#include <vector>

#include "driver_interaction.h"
#include "migration_executor.h"
#include "task_executor.h"

namespace turbo::ucache {
class TurboStrategyExecutor {
public:
    static uint32_t ExecuteMigrationStrategy(MigrationStrategy &strategy);
};
} // namespace turbo::ucache
#endif /* TURBO_STRATEGY_EXECUTOR_H */