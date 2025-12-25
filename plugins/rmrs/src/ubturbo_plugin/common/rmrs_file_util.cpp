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

#include "rmrs_file_util.h"

#include <regex>
#include <cstdlib>
#include <climits>
#include <fstream>
#include <filesystem>
#include <system_error>

#include "turbo_logger.h"
#include "rmrs_config.h"

namespace fs = std::filesystem;
namespace rmrs {
using std::ifstream;
using namespace turbo::log;

/**
 * 判断路径名是否匹配正则
 * @param dirPath const string &: 路径
 * @param pattern const string &: 正则表达式
 * @return
 */
RMRS_RES RmrsFileUtil::IsSpecifiedPath(const string &dirPath, const string &pattern)
{
    const std::regex designPattern(pattern);
    if (!std::regex_match(dirPath, designPattern)) {
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

/**
 * 判断路径是否存在
 * @param path const string &: 路径
 * @return
 */
RMRS_RES RmrsFileUtil::IsPathExist(const string &path)
{
    std::error_code ec;
    if (fs::exists(path, ec)) {
        return RMRS_OK;
    }
    return RMRS_ERROR;
}

/**
 * 读取文件内容
 * @param path const string &: 文件路径
 * @param info vector<string> &: 文件内容, 按行输出
 * @return RMRS_RES
 */
RMRS_RES RmrsFileUtil::GetFileInfo(const string &path, vector<string> &info)
{
    auto realFileName = std::make_unique<char[]>(PATH_MAX);
    if ((realpath(path.c_str(), realFileName.get()) == nullptr) && (errno == ENOENT)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Path is invalid, path: " << path << ".";
        return RMRS_ERROR;
    }
    ifstream file((string(realFileName.get())));
    if (!file.is_open()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Open file failed.";
        return RMRS_ERROR;
    }
    try {
        string line;
        while (getline(file, line)) {
            info.push_back(line);
        }
    } catch (const std::exception &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Read file failed: " << e.what() << ".";
        file.close();
        return RMRS_ERROR;
    }
    file.close();
    return RMRS_OK;
}

} // namespace mempooling