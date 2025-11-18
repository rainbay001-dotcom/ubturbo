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
#include "turbo_conf_manager.h"

#include <fstream>

namespace turbo::config {
using namespace turbo::common;

namespace fs = std::filesystem;

RetCode TurboConfManager::Init(const std::string &confDir, const std::string &libDir)
{
    this->confDir = confDir;
    this->libDir = libDir;

    //  读取 ubturbo.conf
    fs::path absPath;
    auto absPathInit = fs::path(confDir) / fs::path(TURBO_CONF_FILE_NAME);
    std::cout << "[Conf] The absolute path to the configuration file is " << absPathInit << "." << std::endl;
    try {
        absPath = fs::canonical(absPathInit);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[Conf] Turbo conf path resolution failed: " << e.what() << std::endl;
        return TURBO_ERROR;
    }

    std::unordered_map<std::string, std::string> turboConf;
    auto ret = ParseFile(absPath.string(), turboConf);
    if (ret != TURBO_OK) {
        std::cerr << "[Conf] Failed to parse configuration file. Path: " << absPath << "." << std::endl;
        return ret;
    }
    for (const auto &item : turboConf) {
        std::string newKey = absPath.stem().string() + DELIMITER + item.first;
        lineContentMap[newKey] = item.second;
    }

    // 读取 ubturbo_plugin_admission.conf
    std::string pluginConf;
    try {
        pluginConf = fs::canonical(fs::path(confDir) / fs::path(TURBO_PLUGIN_ADMISSION_FILE_NAME)).string();
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[Conf] Turbo plugin addmission path resolution failed: " << e.what() << std::endl;
        return TURBO_ERROR;
    }
    return InitPluginConf(pluginConf);
}

RetCode TurboConfManager::InitPluginConf(const std::string &pluginConfPath)
{
    // 配置文件读取到内存。key 是插件名，value 是插件码
    std::unordered_map<std::string, std::string> pluginAdmissionConf;
    auto ret = ParseFile(pluginConfPath, pluginAdmissionConf);
    if (ret != TURBO_OK) {
        std::cerr << "[Conf] Failed to parse configuration file. Path: " << pluginConfPath << "." << std::endl;
        return ret;
    }

    for (const auto &[pluginName, pluginCodeStr] : pluginAdmissionConf) {
        std::unordered_map<std::string, std::string> pluginConf;
        auto absPath = fs::path(confDir) / fs::path(PLUGIN_CONF_FILE_NAME_PREFIX + pluginName + ".conf");
        ret = ParseFile(absPath.string(), pluginConf);
        if (ret != TURBO_OK) {
            std::cerr << "[Conf] Failed to parse configuration file. Path: " << absPath << "." << std::endl;
            return ret;
        }
        uint16_t pluginCode = 0;
        try {
            pluginCode = turbo::utils::Convert<uint16_t>(pluginCodeStr);
        } catch (...) {
            std::cerr << "[Conf] The code " << pluginCodeStr << " of plugin " << pluginName << " is invalid."
                      << std::endl;
            return TURBO_ERROR;
        }
        if (CheckPluginNameAndCode(pluginName, pluginCode) != TURBO_OK) {
            return TURBO_ERROR;
        }

        //  检查 so 路径
        if (pluginConf.find(KEY_PLUGIN_SO_PATH) == pluginConf.end()) {
            std::cerr << "[Conf] No so path found for plugin " << pluginName << " in configuration file." << std::endl;
            return TURBO_ERROR;
        }
        fs::path soPath;
        try {
            soPath = fs::canonical(fs::path(libDir) / fs::path(pluginConf[KEY_PLUGIN_SO_PATH]));
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Plugin " << pluginName << " so path resolution failed: " << e.what() << ", libdir = "
            << libDir << ", pluginConf = " << pluginConf[KEY_PLUGIN_SO_PATH] << std::endl << ".";
            return TURBO_ERROR;
        }
        // 加入配置文件，plugin 模块会读取并初始化插件
        allPluginConf.push_back({
            .name = pluginName,
            .soPath = soPath.string(),
            .moduleCode = pluginCode,
        });
        //  插件配置文件加入全局配置
        for (const auto &item : pluginConf) {
            std::string newKey = absPath.stem().string() + DELIMITER + item.first;
            lineContentMap[newKey] = item.second;
        }
    }

    return TURBO_OK;
}

RetCode TurboConfManager::ParseFile(const std::string &filePath, std::unordered_map<std::string, std::string> &confMap)
{
    char *canonicalPath = realpath(filePath.c_str(), nullptr);
    if (canonicalPath == nullptr) {
        std::cerr << "[Conf] The path is invalid. Path: " << filePath << "." << std::endl;
        return TURBO_ERROR;
    }

    std::ifstream fileStream(canonicalPath);
    free(canonicalPath);
    RetCode ret = TURBO_OK;

    if (!fileStream.is_open()) {
        std::cerr << "[Conf] Open file failed. Path: " << filePath << "." << std::endl;
        return TURBO_ERROR;
    }

    std::string line;
    std::string key;
    std::string val;
    size_t lineCount = 1;
    while (getline(fileStream, line)) {
        ret = ParseLine(line, lineCount++, key, val);
        if (ret != TURBO_OK) {
            std::cerr << "[Conf] Parse line failed. Line: " << line << ". Path: " << filePath << "." << std::endl;
            return ret;
        }
        if (!key.empty()) {
            // 重复配置项
            if (confMap.find(key) != confMap.end()) {
                std::cerr << "[Conf] Conf file format error, key=" << key << " is repeated. Line: " << lineCount << "."
                          << std::endl;
                return TURBO_ERROR;
            }
            confMap[key] = val;
        }
    }
    return TURBO_OK;
}
RetCode TurboConfManager::ParseLine(std::string line, size_t lineCount, std::string &key, std::string &val)
{
    if (lineCount > CONFIG_MAX_LINES) {
        std::cerr << "[Conf] Configuration file has too many lines: " << lineCount << " (maximum allowed is "
                  << CONFIG_MAX_LINES << ")" << std::endl;

        return TURBO_ERROR;
    }
    LeftTrim(line);
    RightTrim(line);

    // 处理注释行
    if (line.empty() || line.front() == '#' || line.front() == ';') {
        key = "";
        return TURBO_OK;
    }

    // 配置key val 长度检查
    auto [keyTmp, valTmp] = ParseConfigKeyAndValue(line);
    if (keyTmp.size() > CONFIG_MAX_KEY_LEN || keyTmp.size() < CONFIG_MIN_KEY_LEN ||
        valTmp.size() > CONFIG_MAX_VAL_LEN || valTmp.size() < CONFIG_MIN_VAL_LEN) {
        std::cerr << "[Conf] Configuration file format error at line " << lineCount << ": key length exceeds "
                  << CONFIG_MAX_KEY_LEN << " or value length exceeds " << CONFIG_MAX_VAL_LEN << std::endl;

        return TURBO_ERROR;
    }
    // 合法字符检查
    static std::regex keyLegalChars(R"(^[a-zA-Z0-9\.\_\-]+$)");
    if (!std::regex_match(keyTmp, keyLegalChars)) {
        std::cerr << "[Conf] Invalid key " << keyTmp << " in configuration file at line " << lineCount << std::endl;
        return TURBO_ERROR;
    }
    static std::regex valLegalChars(R"(^[a-zA-Z0-9\.\_\-\:\,\/\;]+$)");
    if (!std::regex_match(valTmp, valLegalChars)) {
            std::cerr << "[Conf] Invalid value " << valTmp << " in configuration file at line " << lineCount << "."
                      << std::endl;
            return TURBO_ERROR;
    }
    key = keyTmp;
    val = valTmp;
    return TURBO_OK;
}

RetCode TurboConfManager::GetConf(const std::string &section, const std::string &configKey, std::string &configVal)
{
    auto key = section + DELIMITER + configKey;
    auto iter = lineContentMap.find(key);
    if (iter == lineContentMap.end()) {
        std::cerr << "[Conf] Missing configuration key: " << key << "." << std::endl;
        return TURBO_ERROR;
    }
    
    configVal = iter->second;
    return TURBO_OK;
}

std::unordered_map<std::string, std::string> TurboConfManager::GetAllConf() const
{
    return lineContentMap;
}

void TurboConfManager::Clear()
{
    lineContentMap.clear();
}

std::vector<TurboPluginConf> TurboConfManager::GetAllPluginConf() const
{
    return allPluginConf;
}

RetCode TurboConfManager::CheckPluginNameAndCode(const std::string &pluginName, uint16_t moduleCode)
{
    if (pluginName.empty()) {
        std::cerr << "[Conf] Plugin name is empty." << std::endl;
        return TURBO_ERROR;
    }

    if (moduleCode < MIN_MODULE_CODE) {
        std::cerr << "[Conf] Invalid plugin code: " << moduleCode << ". It must be greater than " << MIN_MODULE_CODE
                  << std::endl;
        return TURBO_ERROR;
    }

    for (const auto &plugin : allPluginConf) {
        if (plugin.name == pluginName) {
            std::cerr << "[Conf] Duplicate plugin name detected: " << pluginName << "." << std::endl;
            return TURBO_ERROR;
        }
        if (plugin.moduleCode == moduleCode) {
            std::cerr << "[Conf] Duplicate plugin code detected: " << moduleCode << "." << std::endl;
            return TURBO_ERROR;
        }
    }

    return TURBO_OK;
}


void LeftTrim(std::string &s, const std::locale &loc)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&loc](char ch) { return !std::isspace(ch, loc); }));
}

void RightTrim(std::string &s, const std::locale &loc)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [&loc](char ch) { return !std::isspace(ch, loc); }).base(), s.end());
}

std::pair<std::string, std::string> ParseConfigKeyAndValue(const std::string &line)
{
    size_t pos = line.find('=');
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    LeftTrim(key);
    RightTrim(key);
    LeftTrim(value);
    RightTrim(value);
    return { key, value };
}

} // namespace turbo::config