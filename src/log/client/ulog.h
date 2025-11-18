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
#ifndef ULOG_H
#define ULOG_H

namespace turbo::smap::ulog {

constexpr int TURBO_MAX_MESSAGE_LENGTH = 256;
constexpr char TURBO_LOG_MODULE_NAME[] = "UBTurboClient";

enum class LoggerLevel : int {
    LOGGER_DEBUG_LEVEL = 0,
    LOGGER_INFO_LEVEL,
    LOGGER_WARNING_LEVEL,
    LOGGER_ERROR_LEVEL
};

#define IPC_CLIENT_LOGGER_DEBUG(...) LoggerMessage(turbo::smap::ulog::LoggerLevel::LOGGER_DEBUG_LEVEL, __VA_ARGS__)
#define IPC_CLIENT_LOGGER_INFO(...) LoggerMessage(turbo::smap::ulog::LoggerLevel::LOGGER_INFO_LEVEL, __VA_ARGS__)
#define IPC_CLIENT_LOGGER_WARNING(...) LoggerMessage(turbo::smap::ulog::LoggerLevel::LOGGER_WARNING_LEVEL, __VA_ARGS__)
#define IPC_CLIENT_LOGGER_ERROR(...) LoggerMessage(turbo::smap::ulog::LoggerLevel::LOGGER_ERROR_LEVEL, __VA_ARGS__)

using Logfunc = void (*)(int level, const char *str, const char *moduleName);

void UpstreamSubscribeLogger(Logfunc extlog);
void LoggerMessage(LoggerLevel severity, const char *fmt, ...);
}

#endif
