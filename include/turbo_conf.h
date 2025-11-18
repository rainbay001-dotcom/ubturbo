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
#ifndef TURBO_CONF_H
#define TURBO_CONF_H
#include <cstdint>
#include <string>

namespace turbo::config {
/**
 * @brief 获取无符号短整型类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configValue [out] 配置参数的值
 * @return #TURBO_OK 0 成功
 * @return #非TURBO_OK 失败
 */
uint32_t UBTurboGetUInt32(const std::string &section, const std::string &configKey, uint32_t &configValue);

/**
 * @brief 获取Float类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configValue [out] 配置参数的值
 * @return #TURBO_OK 0 成功
 * @return #非TURBO_OK 失败
 */
uint32_t UBTurboGetFloat(const std::string &section, const std::string &configKey, float &configValue);

/**
 * @brief 获取字符串类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configValue [out] 配置参数的值
 * @return #TURBO_OK 0 成功
 * @return #非TURBO_OK 失败
 */
uint32_t UBTurboGetStr(const std::string &section, const std::string &configKey, std::string &configValue);

/**
 * @brief 获取布尔类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configValue [out] 配置参数的值
 * @return #TURBO_OK 0 成功
 * @return #非TURBO_OK 失败
 */
uint32_t UBTurboGetBool(const std::string &section, const std::string &configKey, bool &configValue);

/**
 * @brief 获取无符号长整数类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configValue [out] 配置参数的值
 * @return #TURBO_OK 0 成功
 * @return #非TURBO_OK 失败
 */
uint32_t UBTurboGetUInt64(const std::string &section, const std::string &configKey, uint64_t &configValue);

} //  namespace turbo::config

#endif // TURBO_CONF_H
