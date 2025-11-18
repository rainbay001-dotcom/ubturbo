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
#include "turbo_module_plugin.h"

#include "turbo_plugin_manager.h"

namespace turbo::plugin {

using namespace turbo::common;

RetCode TurboModulePlugin::Init()
{
    return TURBO_OK;
}

RetCode TurboModulePlugin::Start()
{
    return TurboPluginManager::GetInstance().Init();
}

void TurboModulePlugin::Stop()
{
    TurboPluginManager::GetInstance().DeInitializePlugins();
}

void TurboModulePlugin::UnInit() {}

std::string TurboModulePlugin::Name()
{
    return "plugin";
}

}  //  namespace turbo::plugin