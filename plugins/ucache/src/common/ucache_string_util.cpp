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

#include "ucache_string_util.h"
#include <cstring>

namespace turbo::ucache {
bool TryExtractValue(const std::string &line, const char *key, uint64_t &outVal)
{
    auto pos = line.find(key);
    if (pos == std::string::npos) {
        return false;
    }

    size_t keyLen = std::strlen(key);
    std::string rawVal = line.substr(pos + keyLen);
    std::string trimmed = TrimString(rawVal);
    if (trimmed.empty()) {
        return false;
    }

    size_t digitEnd = 0;
    while (digitEnd < trimmed.size() && std::isdigit(trimmed[digitEnd])) {
        digitEnd++;
    }
    if (digitEnd == 0) {
        return false;
    }

    std::string numStr = trimmed.substr(0, digitEnd);

    return SafeStoInt(numStr, outVal) == UCACHE_OK;
}
} // namespace turbo::ucache