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
#ifndef TURBO_TYPE_UTIL_H
#define TURBO_TYPE_UTIL_H

#include <string>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <cctype>
#include <algorithm>
#include <limits>
#include <cmath>

namespace turbo::utils {

template <typename T>
bool ConvertToBool(const std::string &str)
{
    std::string lowerStr;
    std::transform(str.begin(), str.end(), std::back_inserter(lowerStr), ::tolower);

    if (lowerStr == "true" || lowerStr == "1" || lowerStr == "yes") {
        return true;
    }
    if (lowerStr == "false" || lowerStr == "0" || lowerStr == "no") {
        return false;
    }
    throw std::invalid_argument("Invalid boolean string: " + str);
}

template <typename T>
T ConvertToUnsignedInt(const std::string &str)
{
    size_t pos = 0;
    unsigned long long value = 0;

    if (!str.empty() && str[0] == '-') {
        throw std::invalid_argument("The uint type does not support negative numbers: " + str);
    }

    try {
        value = std::stoull(str, &pos);
    } catch (...) {
        throw std::invalid_argument("Invalid integer string: " + str);
    }

    if (pos != str.size()) {
        throw std::invalid_argument("Trailing characters in integer string: " + str);
    }

    if (value > static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
        throw std::out_of_range("Value exceeds type limit: " + str);
    }

    return static_cast<T>(value);
}

template <typename T>
T ConvertToFloat(const std::string &str)
{
    size_t pos = 0;
    long double value = 0.0;
    errno = 0;

    try {
        value = std::stold(str, &pos);
    } catch (...) {
        throw std::invalid_argument("Invalid float string: " + str);
    }

    if (pos != str.size()) {
        throw std::invalid_argument("Trailing characters in float string: " + str);
    }

    T valueFinal = static_cast<T>(value);

    if (errno == ERANGE || std::isinf(valueFinal) || std::isnan(valueFinal)) {
        throw std::out_of_range("Floating-point value out of range: " + str);
    }

    return valueFinal;
}

template <typename T>
T Convert(const std::string &s)
{
    // 统一处理前导/后导空白
    std::string str = s;
    str.erase(str.find_last_not_of(" \t\n\r") + 1); // 移除右侧空白
    str.erase(0, str.find_first_not_of(" \t\n\r")); // 移除左侧空白

    if constexpr (std::is_same_v<T, bool>) {
        return ConvertToBool<T>(str);
    } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
        return ConvertToUnsignedInt<T>(str);
    } else if constexpr (std::is_floating_point_v<T>) {
        return ConvertToFloat<T>(str);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return str;
    } else {
        static_assert(sizeof(T) == 0, "Unsupported target type");
    }
}

} // namespace turbo::utils

#endif // TURBO_TYPE_UTIL_H
