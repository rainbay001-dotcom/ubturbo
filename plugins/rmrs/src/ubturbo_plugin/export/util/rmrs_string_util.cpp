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
#include "rmrs_string_util.h"

#include <limits>
#include <regex>
#include <stdexcept>
#include "turbo_logger.h"
#include "rmrs_config.h"

namespace rmrs {

using namespace turbo::log;

pid_t RmrsStringUtil::SafeStopid(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        unsigned long value = std::stoul(str);
        if (value > static_cast<unsigned long>(std::numeric_limits<pid_t>::max())) {
            throw std::out_of_range("Value out of range for pid_t");
        }
        return static_cast<pid_t>(value);
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Invalid argument: " << str << ".";
        return 0;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Out of range: " << str << ".";
        return 0;
    }
}

uint32_t RmrsStringUtil::SafeStoul(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        unsigned long value = std::stoul(str);
        if (value > std::numeric_limits<uint32_t>::max()) {
            throw std::out_of_range("Value out of range for uint32_t");
        }
        return static_cast<uint32_t>(value);
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Invalid argument: " << str << ".";
        return 0;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Out of range: " << str << ".";
        return 0;
    }
}

uint64_t RmrsStringUtil::SafeStoull(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        unsigned long long value = std::stoull(str);
        if (value > std::numeric_limits<uint64_t>::max()) {
            throw std::out_of_range("Value out of range for uint64_t");
        }
        return static_cast<uint64_t>(value);
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Invalid argument: " << str << ".";
        return 0;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Out of range: " << str << ".";
        return 0;
    }
}

int16_t RmrsStringUtil::SafeStoi16(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        int value = std::stoi(str);
        if (value < std::numeric_limits<int16_t>::min() || value > std::numeric_limits<int16_t>::max()) {
            throw std::out_of_range("Value out of range for int16_t");
        }
        return static_cast<int16_t>(value);
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Invalid argument: " << str << ".";
        return 0;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Out of range: " << str << ".";
        return 0;
    }
}

uint16_t RmrsStringUtil::SafeStou16(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        unsigned int value = std::stoul(str);
        if (value > std::numeric_limits<uint16_t>::max()) {
            throw std::out_of_range("Value out of range for uint16_t");
        }
        return static_cast<uint16_t>(value);
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Invalid argument: " << str << ".";
        return 0;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Out of range: " << str << ".";
        return 0;
    }
}

int64_t RmrsStringUtil::SafeStoi64(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        long long value = std::stoll(str);
        if (value < std::numeric_limits<int64_t>::min() || value > std::numeric_limits<int64_t>::max()) {
            throw std::out_of_range("Value out of range for int64_t");
        }
        return static_cast<int64_t>(value);
    } catch (const std::invalid_argument &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Invalid argument: " << str << ".";
        return 0;
    } catch (const std::out_of_range &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Out of range: " << str << ".";
        return 0;
    }
}

RMRS_RES RmrsStringUtil::SafeStof(const std::string &str, float_t &param)
{
    if (str.empty()) {
        return RMRS_ERROR;
    }
    try {
        auto value = std::stof(str);
        param = static_cast<float_t>(value);
        return RMRS_OK;
    } catch (const std::invalid_argument &e) {
        return RMRS_ERROR_INVAL;
    } catch (const std::out_of_range &e) {
        return RMRS_ERROR_EXCEEDS_RANGE;
    }
}

bool RmrsStringUtil::StrToLong(const std::string &src, long &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtol(src.c_str(), &remain, 10L); // 10 is decimal digits
    if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}

bool RmrsStringUtil::StrToUint(const std::string &src, uint32_t &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtoul(src.c_str(), &remain, 10L); // 10 is decimal digits
    if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}

bool RmrsStringUtil::StrToULong(const std::string &src, uint64_t &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtoull(src.c_str(), &remain, 10L); // 10 is decimal digits
    if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}
} // namespace rmrs
