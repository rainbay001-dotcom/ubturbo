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
#ifndef TURBO_MAIN_H
#define TURBO_MAIN_H

#include <iostream>
#include <vector>
#include <chrono>

#include "turbo_module.h"
#include "turbo_common.h"
#include "turbo_error.h"
#include "turbo_module_conf.h"
#include "turbo_module_plugin.h"
#include "turbo_module_logger.h"
#include "turbo_module_ipc.h"

namespace turbo::main {

using namespace turbo::module;

class TurboMain {
public:
    static TurboMain &GetInstance()
    {
        static TurboMain instance;
        return instance;
    }

    TurboMain(const TurboMain &) = delete;
    TurboMain &operator=(const TurboMain &) = delete;

    RetCode Run();
    void Stop();

private:

    TurboMain() = default;
    RetCode InitModule();

    RetCode StartModule();

    void StopModule();

    void UnInitModule();
};

}


#endif //  TURBO_MAIN_H