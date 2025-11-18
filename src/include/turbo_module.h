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
#ifndef TURBO_MODULE_H
#define TURBO_MODULE_H

#include <string>
#include <memory>


#include "turbo_common.h"

namespace turbo::module {
using namespace turbo::common;

// 初始化顺序：所有模块Initialize -> 所有模块Start
// 停止顺序：所有模块Stop -> 所有模块UnInitialize
class TurboModule {
public:
    virtual ~TurboModule() = default;

    virtual RetCode Init() = 0;

    virtual void UnInit() = 0;

    virtual RetCode Start() = 0;

    virtual void Stop() = 0;

    virtual std::string Name() = 0;
};

} //  namespace turbo::module

#endif // TURBO_MODULE_H