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
#ifndef __ULOG4C_H__
#define __ULOG4C_H__

#ifdef __cplusplus
extern "C" {
#endif

int ULOG_Init(int logType, int minLogLevel, const char *path, int rotationFileSize, int rotationFileCount);

int ULOG_LogMessage(int logLevel, const char *prefix, const char *msg);

void ULOG_EXIT(void);

#if defined(__cplusplus)
}
#endif

#endif // __ULOG4C_H__
