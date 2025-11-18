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
#include "turbo_file_util.h"

#include <iostream>
#include <cstring>

namespace turbo::utils {
namespace fs = std::filesystem;


std::string TurboFileUtil::GetExecutablePath()
{
    char filePath[MAX_PATH_LEN];

    // 读取符号链接 获取可执行文件的路径
    const auto count = readlink("/proc/self/exe", filePath, sizeof(filePath) - 1);
    if (count <= 0) {
        std::cerr << "[Conf] Failed to read /proc/self/exe: " << strerror(errno) << " (errno=" << errno << ")"
                  << std::endl;
        return {};
    }

    if (count >= MAX_PATH_LEN - 1) {
        std::cerr << "[Conf] Read /proc/self/exe failed, file path length is too long."
                  << std::endl;
        return {};
    }
    filePath[count] = '\0'; // 确保字符串以 null 结尾
    return {filePath};
}

std::string TurboFileUtil::GetExecutableRootDir()
{
    std::string exePathStr = GetExecutablePath();
    return fs::path(exePathStr).parent_path().parent_path().string();
}

} //  namespace turbo::utils