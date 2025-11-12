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

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#ifndef USE_DT
#include "ulog4c.h"
#endif
#include "securec.h"
#include "smap_user_log.h"

#define SMAP_MAX_FILE_SIZE (200 * 1024 * 1024) // 每个日志文件的最大大小
#define SMAP_MAX_FILE_NUM 50

#define SMAP_LOG_BUF_LEN 512

#define ULOG_TYPE_FILE 1

#define SMAP_LOG_MODULE_NAME "SMAP "

typedef enum {
    ULOG_LEVEL_TRACE = 0,
    ULOG_LEVEL_DEBUG = 1,
    ULOG_LEVEL_INFO = 2,
    ULOG_LEVEL_WARN = 3,
    ULOG_LEVEL_ERROR = 4,
    ULOG_LEVEL_CRITICAL = 5,
    ULOG_LEVEL_BUTT
} UlogLevel;

typedef enum {
    EXT_LOGGER_DEBUG = 0,
    EXT_LOGGER_INFO,
    EXT_LOGGER_WARNING,
    EXT_LOGGER_ERROR,
} ExtLoggerLevel;

typedef void (*Logfunc)(int level, const char *str, const char *moduleName);

static Logfunc g_subscribers;

void UpstreamSubscribeLogger(Logfunc extlog)
{
    g_subscribers = extlog;
}

int SmapStartULog(const char *ulogPath)
{
    char logBuf[SMAP_LOG_BUF_LEN];
    int ret;

    if (ulogPath == NULL) {
        return -EINVAL;
    }

    ret = sprintf_s(logBuf, SMAP_LOG_BUF_LEN, "%s", ulogPath);
    if (ret < 0) {
        return -1;
    }

#ifndef USE_DT
    ret = ULOG_Init(ULOG_TYPE_FILE, ULOG_LEVEL_DEBUG, logBuf, SMAP_MAX_FILE_SIZE, SMAP_MAX_FILE_NUM);
    if (ret != 0) {
        return -EIO;
    }
#endif

    return 0;
}

static int LogLevelMapping(int logLevel)
{
    switch (logLevel) {
        case ULOG_LEVEL_TRACE:
        case ULOG_LEVEL_DEBUG:
            return EXT_LOGGER_DEBUG;
        case ULOG_LEVEL_INFO:
            return EXT_LOGGER_INFO;
        case ULOG_LEVEL_WARN:
            return EXT_LOGGER_WARNING;
        case ULOG_LEVEL_ERROR:
        default:
            return EXT_LOGGER_ERROR;
    }
}

static void SmapUlogInner(int logLevel, const char *prefix, const char *msg)
{
    if (g_subscribers != NULL) {
        int level = LogLevelMapping(logLevel);
        char exLogMsg[SMAP_LOG_BUF_LEN] = SMAP_LOG_MODULE_NAME;
        errno_t ret = strncat_s(exLogMsg, sizeof(exLogMsg), prefix, SMAP_LOG_BUF_LEN - 1);
        if (ret) {
            return;
        }
        g_subscribers(level, msg, exLogMsg);
        return;
    }
#ifndef USE_DT
    ULOG_LogMessage(logLevel, prefix, msg);
#endif
}

void SMAP_Ulog(int logLevel, const char *funcName, int funcLine, const char *fileName, const char *format, ...)
{
    int ret;
    va_list argPtr;
    char head[SMAP_LOG_BUF_LEN] = { 0 };
    char data[SMAP_LOG_BUF_LEN] = { 0 };

    if (snprintf_s(head, sizeof(head), sizeof(head) - 1, "%s %d %s", fileName, funcLine, funcName) == 0) {
        return;
    }
    va_start(argPtr, format);
    ret = vsnprintf_s(data, SMAP_LOG_BUF_LEN, sizeof(data) - 1, format, argPtr);
    if (ret < 0) {
#ifndef USE_DT
        ULOG_LogMessage(ULOG_LEVEL_ERROR, head, "vsnprintf_s failed. ");
#endif
        va_end(argPtr);
        return;
    }
    va_end(argPtr);

    switch (logLevel) {
        case SMAP_LOG_INFO:
            SmapUlogInner(ULOG_LEVEL_INFO, head, data);
            break;
        case SMAP_LOG_WARNING:
            SmapUlogInner(ULOG_LEVEL_WARN, head, data);
            break;
        case SMAP_LOG_ERROR:
            SmapUlogInner(ULOG_LEVEL_ERROR, head, data);
            break;
        case SMAP_LOG_DEBUG:
            SmapUlogInner(ULOG_LEVEL_DEBUG, head, data);
            break;
        default:
            break;
    }

    return;
}

void SmapLoggerExit(void)
{
    if (g_subscribers != NULL) {
        g_subscribers = NULL;
    }
}
