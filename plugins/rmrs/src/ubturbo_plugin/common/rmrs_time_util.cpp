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
#include "rmrs_time_util.h"

#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>

#include <rmrs_error.h>

namespace rmrs {
using std::localtime;
using std::strftime;
using std::string;

RMRS_RES RmrsTimeUtil::CharPtrToTime(const char *timeChar, time_t &t, DateFormatMode mode)
{
    if (timeChar == nullptr) {
        return RMRS_ERROR;
    }
    tm tmpTm{};
    std::istringstream ss(timeChar);
    ss >> std::get_time(&tmpTm, Convert(mode).c_str());
    t = mktime(&tmpTm);
    return RMRS_OK;
}

std::string RmrsTimeUtil::Convert(DateFormatMode mode)
{
    switch (mode) {
        case DateFormatMode::SPACE_SEPARATOR_MODE:
            return "%Y-%m-%d %H:%M:%S";
        case DateFormatMode::T_SEPARATOR_MODE:
            return "%Y-%m-%dT%H:%M:%S";
    }
    return {};
}

time_t RmrsTimeUtil::GetCurrentTimeT()
{
    auto currentTime = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime.time_since_epoch());
    return seconds.count();
}

time_t RmrsTimeUtil::GetOneHourAgoTimeT()
{
    auto oneHourAgo = std::chrono::system_clock::now() - std::chrono::hours(1);
    auto hours = std::chrono::duration_cast<std::chrono::hours>(oneHourAgo.time_since_epoch());
    return hours.count();
}
} // rmrs
