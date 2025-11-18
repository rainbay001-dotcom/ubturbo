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
#ifndef RMRS_STRING_UTIL_H
#define RMRS_STRING_UTIL_H

#include <cmath>
#include <string>
#include <vector>
#include <set>
#include <cstring>

#include "rmrs_error.h"

namespace rmrs {
class RmrsStringUtil {
public:
    static pid_t SafeStopid(const std::string &str);
    static uint32_t SafeStoul(const std::string &str);
    static uint64_t SafeStoull(const std::string &str);
    static int16_t SafeStoi16(const std::string &str);
    static uint16_t SafeStou16(const std::string &str);
    static int64_t SafeStoi64(const std::string &str);
    static RMRS_RES SafeStof(const std::string &str, float_t &param);

    static bool StrToLong(const std::string &src, long &value);
    static bool StrToUint(const std::string &src, uint32_t &value);
    static bool StrToULong(const std::string &src, uint64_t &value);

    static void Replace(std::string &src, const std::string &regex, const std::string &replaced);
};
}

#endif // RMRS_STRING_UTIL_H
