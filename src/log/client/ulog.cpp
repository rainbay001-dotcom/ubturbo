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
#include <cstdarg>
#include <string>
#include <sstream>

#include "securec.h"
#include "ulog.h"

namespace turbo::smap::ulog {

static Logfunc g_extlog;

void UpstreamSubscribeLogger(Logfunc extlog)
{
    g_extlog = extlog;
}

void LoggerMessage(LoggerLevel severity, const char *fmt, ...)
{
    char gMessage[TURBO_MAX_MESSAGE_LENGTH];
    va_list ap;
    int level = static_cast<int>(severity);
    va_start(ap, fmt);
    int ret = vsnprintf_s(gMessage, TURBO_MAX_MESSAGE_LENGTH,
        TURBO_MAX_MESSAGE_LENGTH - 1, fmt, ap);
    if (ret < 0) {
        va_end(ap);
        return;
    }
    va_end(ap);
    if (g_extlog != nullptr) {
        g_extlog(level, gMessage, TURBO_LOG_MODULE_NAME);
    }
}

}