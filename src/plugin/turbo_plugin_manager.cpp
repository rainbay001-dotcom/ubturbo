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
#include "turbo_plugin_manager.h"

#include "turbo_error.h"
#include "turbo_conf.h"

#include <dlfcn.h>


namespace turbo::plugin {
using namespace turbo::common;
using namespace turbo::config;

RetCode TurboPluginManager::Init()
{
    LoadAndInitPlugins();
    return TURBO_OK;
}

void TurboPluginManager::DeInitializePlugins()
{
    for (auto it = loadedPluginModules.begin(); it != loadedPluginModules.end();) {
        // 边遍历边删除的情况下，需要先保存下一个迭代器
        auto current = it;
        it++;

        DeInitializePlugin(current->first);
    }
}

void TurboPluginManager::LoadAndInitPlugins()
{
    for (const auto &plugin : TurboConfManager::GetInstance().GetAllPluginConf()) {
        const std::string &pluginName = plugin.name;
        UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Plugin] Starting to load plugin, plugin name: " << pluginName;
        if (LoadAndInitPlugin(plugin) != TURBO_OK) {
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
                << "[Plugin] Failed to load and initialize plugin: " << pluginName << ".";
            continue;
        }
        UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Plugin] Plugin \"" << pluginName << "\" loaded successfully.";
    }
}

RetCode TurboPluginManager::LoadAndInitPlugin(const TurboPluginConf &pluginConf)
{
    auto ret = LoadPlugin(pluginConf.name, pluginConf.soPath);
    if (ret != TURBO_OK) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Plugin] Failed to load plugin: " << pluginConf.name << ".";
        return ret;
    }

    ret = InitPlugin(pluginConf.name, pluginConf.moduleCode);
    if (ret != TURBO_OK) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Plugin] Failed to initialize  plugin: " << pluginConf.name << ".";
        return ret;
    }

    return TURBO_OK;
}

RetCode TurboPluginManager::LoadPlugin(const std::string &pluginName, const std::string &fileName)
{
    if (loadedPluginModules.find(pluginName) != loadedPluginModules.end()) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Plugin] Plugin \"" << pluginName << "\" is already loaded.";
        return TURBO_ERROR;
    }

    char *canonicalPath = realpath(fileName.c_str(), nullptr);
    if (canonicalPath == nullptr) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Plugin] The path of the so file corresponding to the plugin "
                                                  << pluginName << " is invalid, file: " << fileName;
        return TURBO_ERROR;
    }

    void *handle = dlopen(canonicalPath, RTLD_NOW | RTLD_GLOBAL);
    free(canonicalPath);
    if (handle == nullptr) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Plugin] Failed to load plugin " << pluginName << " so, error: " << dlerror();
        return TURBO_ERROR;
    }
    
    loadedPluginModules[pluginName] = handle;
    return TURBO_OK;
}

RetCode TurboPluginManager::InitPlugin(const std::string &pluginName, const uint16_t &moduleCode)
{
    auto initFunc = GetInitFunction(pluginName, "TurboPluginInit");
    if (initFunc == nullptr) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Plugin] Failed to find initialization function \"TurboPluginInit\" in plugin \"" << pluginName
            << "\".";
        return TURBO_ERROR;
    }

    uint32_t ret = initFunc(moduleCode);
    if (ret != TURBO_OK) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Plugin] Failed to initialize plugin " << pluginName << ", error code: " << ret;
        return TURBO_ERROR;
    }

    return TURBO_OK;
}

RetCode TurboPluginManager::DeInitializePlugin(const std::string &pluginName)
{
    auto it = loadedPluginModules.find(pluginName);
    if (it == loadedPluginModules.end()) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Plugin] Plugin " << pluginName << " not found.";
        return TURBO_ERROR;
    }

    auto handler = it->second;
    if (handler) {
        UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Plugin] Starting to unload plugin: " << pluginName << ".";
        auto deInitFunc  = (TurboPluginDeInitFunc)dlsym(handler, "TurboPluginDeInit");
        if (deInitFunc) {
            deInitFunc();
            UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE)
                << "[Plugin] De-initialization function succeeded for plugin " << pluginName << ".";
        } else {
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
                << "[Plugin] De-initialization function not found for plugin: " << pluginName << ".";
        }
        //  卸载动态库
        int ret = dlclose(handler);
        if (ret != 0) {
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
                << "[Plugin] Failed to close plugin " << pluginName << " so, error: " << dlerror();
            return TURBO_ERROR;
        }
    }
    loadedPluginModules.erase(it);
    return TURBO_OK;
}


TurboPluginInitFunc TurboPluginManager::GetInitFunction(const std::string &pluginName, const std::string &funcName)
{
    void *handle = loadedPluginModules[pluginName];
    if (handle == nullptr) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Plugin] Invalid handle for plugin " << pluginName << ".";
        return nullptr;
    }

    auto func = (TurboPluginInitFunc)(dlsym(handle, funcName.c_str()));
    if (func == nullptr) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Plugin] Failed to get TurboPluginInit for plugin " << pluginName << ", error: " << dlerror();
    }
    return func;
}

} //  namespace turbo::plugin
