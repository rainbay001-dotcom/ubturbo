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
#ifndef __ULOG_H__
#define __ULOG_H__

#include <cstdint>
#include <string>
#include <sstream>

#include "spdlog/common.h"
#include "spdlog/spdlog.h"

namespace utilities {
namespace log {
class ULog {
public:
    ULog(int logType, int minLogLevel, const std::string &path, int rotationFileSize, int rotationFileCount)
        : logType(logType),
          minLogLevel(minLogLevel),
          filePath(path),
          rotationFileSize(rotationFileSize),
          rotationFileCount(rotationFileCount)
    {}

    ~ULog() = default;

    int Initialize();

    inline bool IsDebug() const
    {
        return debugEnabled;
    }

    inline bool IsHigherLevel(int nowLevel) const
    {
        return nowLevel >= minLogLevel;
    }

    int Log(int level, const char *prefix, const char *message) const;

    static int CreateSingleUlog(int type, int minLevel, const char *path, int fileSize, int fileCount);
    static int LogMessage(int level, const char *prefix, const char *message);
    static void Exit();
    static ULog *gSingleLogger;

    std::shared_ptr<spdlog::logger> mSPDLogger;

private:
    static int VerifyUlogParamters(int type, int minLevel, const char *path, int fileSize, int fileCount);

private:
    int logType;
    int minLogLevel;
    std::string filePath;
    int rotationFileSize;
    int rotationFileCount;
    bool debugEnabled = false;

    static thread_local std::string gLastErrorMessage;
};
}
}

#endif
