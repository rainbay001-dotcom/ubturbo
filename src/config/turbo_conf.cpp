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
#include "turbo_conf.h"

#include <string>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <cctype>
#include <algorithm>
#include <limits>

#include "turbo_common.h"
#include "turbo_error.h"
#include "turbo_conf_manager.h"
#include "turbo_type.h"

namespace turbo::config {
using namespace turbo::common;
using namespace turbo::utils;

template <typename T>
RetCode TurboGet(const std::string &section, const std::string &configKey, T &configValue)
{
    std::string strVal;
    auto ret = TurboConfManager::GetInstance().GetConf(section, configKey, strVal);
    if (ret != 0) {
        std::cerr << "[Conf] Get conf failed, section: " << section << ", key: " << configKey << std::endl;
        return ret;
    }

    try {
        configValue = Convert<T>(strVal);
    } catch (const std::invalid_argument &e) {
        std::cerr << e.what() << std::endl;
        return TURBO_ERROR;
    } catch (const std::out_of_range &e) {
        std::cerr << e.what() << std::endl;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}


uint32_t UBTurboGetUInt32(const std::string &section, const std::string &configKey, uint32_t &configValue)
{
    return TurboGet<uint32_t>(section, configKey, configValue);
}

uint32_t UBTurboGetFloat(const std::string &section, const std::string &configKey, float &configValue)
{
    return TurboGet<float>(section, configKey, configValue);
}

uint32_t UBTurboGetStr(const std::string &section, const std::string &configKey, std::string &configValue)
{
    return TurboGet<std::string>(section, configKey, configValue);
}

uint32_t UBTurboGetBool(const std::string &section, const std::string &configKey, bool &configValue)
{
    return TurboGet<bool>(section, configKey, configValue);
}

uint32_t UBTurboGetUInt64(const std::string &section, const std::string &configKey, uint64_t &configValue)
{
    return TurboGet<uint64_t>(section, configKey, configValue);
}

} // namespace turbo::config