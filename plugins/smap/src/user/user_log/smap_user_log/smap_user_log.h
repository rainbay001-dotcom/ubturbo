/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef SMAP_USER_LOG_H
#define SMAP_USER_LOG_H

#include <string.h>

#define SMAP_LOG_INTERVAL_TIME 30
#define SMAP_LOG_BURST_NUM 3

#ifndef FILENAME
#define FILENAME (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1) : __FILE__)
#endif

#ifndef SMAP_LOG_FILE_PATH
#define SMAP_LOG_FILE_PATH "/var/log/smap_log"
#endif

typedef enum SmapLogLevel {
    SMAP_LOG_DEBUG = 0,
    SMAP_LOG_INFO,
    SMAP_LOG_WARNING,
    SMAP_LOG_ERROR,
    SMAP_LOG_BUTT
} SmapLogLevel;

typedef void (*Logfunc)(int level, const char *str, const char *moduleName);

void UpstreamSubscribeLogger(Logfunc extlog);
int SmapStartULog(const char *ulogPath);
void SmapLoggerExit(void);
void SMAP_Ulog(int logLevel, const char *funcName, int funcLine, const char *fileName, const char *format, ...);

#ifdef USE_DT
#define SMAP_LOGGER_DEBUG(fmt, ...)
#define SMAP_LOGGER_INFO(fmt, ...)
#define SMAP_LOGGER_WARNING(fmt, ...)
#define SMAP_LOGGER_ERROR(fmt, ...)
#else

#ifndef RELEASE
#define SMAP_LOGGER_DEBUG(fmt, ...)                                                      \
    do {                                                                                 \
        SMAP_Ulog(SMAP_LOG_DEBUG, __FUNCTION__, __LINE__, FILENAME, fmt, ##__VA_ARGS__); \
    } while (0)
#else

#define SMAP_LOGGER_DEBUG(fmt, ...)
#endif

#define SMAP_LOGGER_INFO(fmt, ...)                                                      \
    do {                                                                                \
        SMAP_Ulog(SMAP_LOG_INFO, __FUNCTION__, __LINE__, FILENAME, fmt, ##__VA_ARGS__); \
    } while (0)

#define SMAP_LOGGER_WARNING(fmt, ...)                                                      \
    do {                                                                                   \
        SMAP_Ulog(SMAP_LOG_WARNING, __FUNCTION__, __LINE__, FILENAME, fmt, ##__VA_ARGS__); \
    } while (0)

#define SMAP_LOGGER_ERROR(fmt, ...)                                                      \
    do {                                                                                 \
        SMAP_Ulog(SMAP_LOG_ERROR, __FUNCTION__, __LINE__, FILENAME, fmt, ##__VA_ARGS__); \
    } while (0)
#endif

#endif
