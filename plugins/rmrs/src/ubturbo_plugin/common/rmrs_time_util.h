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
#ifndef RMRS_TIME_UTIL_H
#define RMRS_TIME_UTIL_H

#include <ctime>
#include <string>

#include "rmrs_error.h"

namespace rmrs {
enum class DateFormatMode {
    SPACE_SEPARATOR_MODE = 0, // %Y-%m-%d %H:%M:%S
    T_SEPARATOR_MODE          // %Y-%m-%dT%H:%M:%S
};

class RmrsTimeUtil {
public:
    static const size_t timeChartSize = 20;

    static RMRS_RES CharPtrToTime(const char *timeChar, time_t &t, DateFormatMode mode);

    static time_t GetCurrentTimeT();

    static time_t GetOneHourAgoTimeT();

private:
    static std::string Convert(DateFormatMode mode);
};
}

#endif // RMRS_TIME_UTIL_H