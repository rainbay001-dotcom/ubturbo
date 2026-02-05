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

#ifndef UCACHE_STRING_UTIL_H
#define UCACHE_STRING_UTIL_H

#include <charconv>
#include <string>
#include <limits>
#include "ucache_turbo_error.h"

namespace turbo::ucache {

template <typename T>
uint32_t SafeStoInt(const std::string &s, T &out)
{
    if (s.empty()) {
        return EMPTY_STRING;
    }

    using WideT = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;
    WideT temp = 0;

    auto result = std::from_chars(s.data(), s.data() + s.size(), temp);
    if (result.ec != std::errc{}) {
        return INVALID_ARGUMENT;
    }

    if (result.ptr != s.data() + s.size()) {
        return INVALID_ARGUMENT;
    }

    if (temp < std::numeric_limits<T>::min() || temp > std::numeric_limits<T>::max()) {
        return OUT_OF_RANGE;
    }

    out = static_cast<T>(temp);
    return UCACHE_OK;
}

inline std::string TrimString(const std::string &str)
{
    static const char *kWhitespace = " \t\n\r\f\v";

    size_t start = str.find_first_not_of(kWhitespace);
    if (start == std::string::npos) {
        return "";
    }

    size_t end = str.find_last_not_of(kWhitespace);
    return str.substr(start, end - start + 1);
}

bool TryExtractValue(const std::string &line, const char *key, uint64_t &outVal);
} // namespace turbo::ucache

#endif // UCACHE_STRING_UTIL_H