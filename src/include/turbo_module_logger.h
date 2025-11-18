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
#ifndef UBTURBO_TURBO_MODULE_LOGGER_H
#define UBTURBO_TURBO_MODULE_LOGGER_H

#include "turbo_common.h"
#include "turbo_module.h"

namespace turbo::log {
using namespace turbo::common;
using namespace turbo::module;

class TurboModuleLogger : public TurboModule {
public:
    RetCode Init() override;

    void UnInit() override;

    RetCode Start() override;

    void Stop() override;

    std::string Name() override;
};

}
#endif  // UBTURBO_TURBO_MODULE_LOGGER_H
