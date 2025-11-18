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
#ifndef TURBO_CONFIG_MANAGER_H
#define TURBO_CONFIG_MANAGER_H
#include <unordered_map>
#include <locale>
#include <vector>
#include <filesystem>
#include <iostream>
#include <regex>

#include "turbo_common.h"
#include "turbo_error.h"
#include "turbo_type.h"


namespace turbo::config {
using namespace turbo::common;

constexpr auto TURBO_CONF_FILE_NAME = "ubturbo.conf";
constexpr int CONFIG_MAX_LINES = 1000;      //  配置文件最大行数
constexpr int CONFIG_MAX_KEY_LEN = 256;     //  配置文件最大key长度
constexpr int CONFIG_MIN_KEY_LEN = 1;       //  配置文件最小key长度
constexpr int CONFIG_MAX_VAL_LEN = 256;     //  配置文件最大val长度
constexpr int CONFIG_MIN_VAL_LEN = 1;       //  配置文件最小val长度
constexpr auto DELIMITER = "@";

// plugin 配置相关
constexpr auto TURBO_PLUGIN_ADMISSION_FILE_NAME = "ubturbo_plugin_admission.conf";
constexpr auto PLUGIN_CONF_FILE_NAME_PREFIX = "plugin_";
constexpr auto KEY_PLUGIN_SO_PATH = "turbo.plugin.pkg";
constexpr auto MIN_MODULE_CODE  = 200;

struct TurboPluginConf {
    std::string name;
    std::string soPath;
    uint16_t moduleCode;
};

class TurboConfManager {
public:
    static TurboConfManager &GetInstance(void)
    {
        static TurboConfManager instance;
        return instance;
    }
    TurboConfManager operator = (const TurboConfManager &) = delete;
    TurboConfManager(const TurboConfManager &) = delete;

     /**
     * @brief 从配置文件读取配置
     * @param[in] confDir: 配置文件所在的目录
     * @param[in] libDir: 插件so文件所在的目录
     * @return RetCode, 成功返回0, 失败返回非0
     */
    RetCode Init(const std::string &confDir, const std::string &libDir);

    /**
     * @brief 获取单条配置
     * @param[in] section: 配置节
     * @param[in] configKey: 配置参数key
     * @param[out] configVal: 配置参数值
     * @return RetCode, 成功返回0, 失败返回非0
     */
    RetCode GetConf(const std::string &section, const std::string &configKey, std::string &configVal);
     
    /**
     * @brief 获取所有配置
     * @param[out] kvs: 配置参数值
     * @return RetCode, 成功返回0, 失败返回非0
     */
    std::unordered_map<std::string, std::string> GetAllConf() const;

    /**
     * @brief 清理配置数据
     */
    void Clear();

    /**
     * @brief 获取所有插件配置
     */
    std::vector<TurboPluginConf> GetAllPluginConf() const;

private:
    TurboConfManager() = default;
    RetCode InitPluginConf(const std::string &pluginConfPath);
    RetCode ParseFile(const std::string &filePath, std::unordered_map<std::string, std::string> &confMap);
    RetCode ParseLine(std::string line, size_t lineCount, std::string &key, std::string &val);
    RetCode CheckPluginNameAndCode(const std::string &pluginName, uint16_t moduleCode);

    std::string confDir;
    std::string libDir;
    std::unordered_map<std::string, std::string> lineContentMap;
    std::vector<TurboPluginConf> allPluginConf;
};

void LeftTrim(std::string &s, const std::locale &loc = std::locale{ "C" }); // 修剪左端空白字符

void RightTrim(std::string &s, const std::locale &loc = std::locale{ "C" }); // 修剪右端空白字符

std::pair<std::string, std::string> ParseConfigKeyAndValue(const std::string &line);

} //  namespace turbo::config


#endif // TURBO_CONFIG_MANAGER_H