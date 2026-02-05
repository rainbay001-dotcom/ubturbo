/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * ucache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ucache_file_util.h"

#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>

#include "turbo_logger.h"
#include "ucache_turbo_config.h"
#include "ucache_turbo_error.h"

namespace turbo::ucache {
using std::ifstream;
using namespace turbo::log;

/**
 * 读取文件内容
 * @param pathStr const string &: 文件路径
 * @param info vector<string> &: 文件内容, 按行输出
 * @return uint32_t
 */
uint32_t UCacheFileUtil::GetFileInfo(const std::string &pathStr, std::vector<std::string> &info)
{
    if (pathStr.empty()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Empty filepath.";
        return UCACHE_ERR;
    }
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path p = fs::canonical(pathStr, ec);
    if (ec) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid filepath, error: " << ec.message() << ".";
        return UCACHE_ERR;
    }

    if (!fs::is_regular_file(p, ec)) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Path is not a regular file.";
        return UCACHE_ERR;
    }

    ifstream file(p);
    if (!file.is_open()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Open file failed.";
        return UCACHE_ERR;
    }

    string line;
    size_t lineCount = 0;
    const size_t maxLines = 10000;

    while (getline(file, line)) {
        if (++lineCount > maxLines) {
            UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) <<
                "File too large, truncated at " << maxLines << " lines.";
            break;
        }
        info.push_back(line);
    }

    return UCACHE_OK;
}

uint32_t UCacheFileUtil::ListDirectory(const string &path, vector<string> &outNames, bool sorted)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
        return UCACHE_ERR;
    }

    fs::directory_iterator dirIter(path, ec);
    if (ec) {
        return UCACHE_ERR;
    }

    for (const auto &entry : dirIter) {
        outNames.emplace_back(entry.path().filename().string());
    }

    if (sorted && !outNames.empty()) {
        std::sort(outNames.begin(), outNames.end());
    }

    return UCACHE_OK;
}
} // namespace turbo::ucache