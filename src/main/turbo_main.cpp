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
#include "turbo_main.h"
#include "turbo_module_smap.h"

namespace turbo::main {
using namespace turbo::module;

// 这里的顺序就是启动顺序，停止顺序与之相反。模块间有依赖关系，需要注意
const std::vector<std::shared_ptr<TurboModule>> g_modules = {
    std::make_shared<turbo::config::TurboModuleConf>(),
    std::make_shared<turbo::log::TurboModuleLogger>(),
    std::make_shared<turbo::smap::TurboModuleSmap>(),
    std::make_shared<turbo::plugin::TurboModulePlugin>(),
    std::make_shared<turbo::ipc::server::TurboModuleIPC>()
};


RetCode TurboMain::Run()
{
    std::cout << "[Main] TurboMain::Run start" << std::endl;
    auto startTime = std::chrono::system_clock::now();

    RetCode ret = InitModule();
    if (ret != TURBO_OK) {
        std::cerr << "[Main] InitModule failed" << std::endl;
        return ret;
    }

    ret = StartModule();
    if (ret != TURBO_OK) {
        std::cerr << "[Main] StartModule failed" << std::endl;
        return ret;
    }

    auto endTime = std::chrono::system_clock::now();
    std::cout << "[Main] TurboMain::Run end, cost time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms"
              << std::endl;
    return TURBO_OK;
}

void TurboMain::Stop()
{
    std::cout << "[Main] TurboMain::Stop start" << std::endl;
    auto startTime = std::chrono::system_clock::now();
    
    StopModule();
    UnInitModule();

    auto endTime = std::chrono::system_clock::now();
    std::cout << "[Main] TurboMain::Stop end, cost time: " <<
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
        << "ms" << std::endl;
}

RetCode TurboMain::InitModule()
{
    RetCode ret = TURBO_OK;
    
    std::cout << "[Main] TurboMain::InitModule start" << std::endl;

    for (const auto &m : g_modules) {
        ret = m->Init();
        if (ret != TURBO_OK) {
            std::cerr << "[Main] Init module failed, name:" << m->Name() << std::endl;
            return ret;
        }
    }
    std::cout << "[Main] TurboMain::InitModule end" << std::endl;
    return TURBO_OK;
}

RetCode TurboMain::StartModule()
{
    RetCode ret = TURBO_OK;

    std::cout << "[Main] TurboMain::StartModule start" << std::endl;
    for (const auto &m : g_modules) {
        ret = m->Start();
        if (ret != TURBO_OK) {
            std::cerr << "[Main] Start module failed, name:" << m->Name() << std::endl;
            return ret;
        }
    }
    std::cout << "[Main] TurboMain::StartModule end" << std::endl;
    return TURBO_OK;
}

void TurboMain::StopModule()
{
    std::cout << "[Main] TurboMain::StopModule start" << std::endl;
    for (auto m = g_modules.rbegin(); m != g_modules.rend(); ++m) {
        // 这里可能需要做异常处理
        (*m)->Stop();
        std::cout << "[Main] Stop module:" << (*m)->Name() << std::endl;
    }
    std::cout << "[Main] TurboMain::StopModule end" << std::endl;
}

void TurboMain::UnInitModule()
{
    std::cout << "[Main] TurboMain::UnInitModule start" << std::endl;
    for (auto m = g_modules.rbegin(); m != g_modules.rend(); ++m) {
        (*m)->UnInit();
        std::cout << "[Main] UnInit module:" << (*m)->Name() << std::endl;
    }
    std::cout << "[Main] TurboMain::UnInitModule end" << std::endl;
}

} //  namespace turbo::main