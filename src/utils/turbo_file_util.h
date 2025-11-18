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
#ifndef TURBO_FILE_UTIL_H
#define TURBO_FILE_UTIL_H

#include <regex>
#include <string>
#include <unistd.h>
#include <limits>
#include <filesystem>


#include "turbo_common.h"

namespace turbo::utils {
using namespace turbo::common;

constexpr auto MAX_PATH_LEN = 4096;

class TurboFileUtil {
public:
    static std::string GetExecutablePath();

    static std::string GetExecutableRootDir();
};
} // namespace turbo::utils

#endif // TURBO_FILE_UTIL_H
