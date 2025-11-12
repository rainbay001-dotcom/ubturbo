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
#include <string>
#include <cstring>

#include "ulog.h"
#include "ulog_errno.h"
#include "ulog4c.h"

#ifdef __cplusplus
extern "C" {
#endif

int ULOG_Init(int logType, int minLogLevel, const char *path, int rotationFileSize, int rotationFileCount)
{
    return utilities::log::ULog::CreateSingleUlog(logType, minLogLevel, path, rotationFileSize, rotationFileCount);
}

int ULOG_LogMessage(int logLevel, const char *prefix, const char *msg)
{
    return utilities::log::ULog::LogMessage(logLevel, prefix, msg);
}

void ULOG_EXIT(void)
{
    utilities::log::ULog::Exit();
}

#if defined(__cplusplus)
}
#endif