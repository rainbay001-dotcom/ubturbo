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
#ifndef TURBO_PLUGIN_MANAGER_H
#define TURBO_PLUGIN_MANAGER_H

#include "turbo_common.h"
#include "turbo_conf_manager.h"
#include "turbo_logger.h"

#include <string>
#include <map>
#include <iostream>

namespace turbo::plugin {
using namespace turbo::common;
using namespace turbo::config;

using TurboPluginInitFunc = uint32_t (*)(const uint16_t);
using TurboPluginDeInitFunc = void (*)();

class TurboPluginManager {
public:
    static TurboPluginManager &GetInstance(void)
    {
        static TurboPluginManager instance;
        return instance;
    }
    TurboPluginManager operator = (const TurboPluginManager &) = delete;
    TurboPluginManager(const TurboPluginManager &) = delete;

    /**
     * @brief 插件管理模块启动:读取插件配置和准入配置，并初始化所有插件
     * @return RetCode, 成功返回0, 失败返回非0
     */
    RetCode Init();

    /* *
     * 卸載所有插件,调用去初始化函数，并关闭so文件
     */
    void DeInitializePlugins();

private:
    TurboPluginManager() = default;

    /* *
     * 加载、初始化所有允许的插件
     * @return RetCode
     */
    void LoadAndInitPlugins();

    /* *
     * 单独加载并初始化某个插件
     * @param pluginName 插件名称
     * @param moduleCode 模块编码
     * @return RetCode
     */
    RetCode LoadAndInitPlugin(const TurboPluginConf &pluginConf);

    /* *
     * 加载指定插件的模块（so文件）。
     * @param pluginName  插件名称
     * @param fileName so文件路径
     * @return RetCode
     */
    RetCode LoadPlugin(const std::string &pluginName, const std::string &fileName);

     /* *
     * 初始化插件
     * @param pluginName  插件名称
     * @param moduleCode 模块编码
     * @return RetCode
     */
    RetCode InitPlugin(const std::string &pluginName, const uint16_t &moduleCode);

    /* *
     * 卸载单个插件，调用趋势化函数，并关闭so文件
     * @param pluginName 插件名称
     * @return RetCode
     */
    RetCode DeInitializePlugin(const std::string &pluginName);

    TurboPluginInitFunc GetInitFunction(const std::string &pluginName, const std::string &funcName);

    // 存储所有已加载插件的模块句柄
    std::map<std::string, void *> loadedPluginModules;
};

}
#endif //  TURBO_PLUGIN_MANAGER_H