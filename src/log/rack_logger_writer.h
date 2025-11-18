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
#ifndef TURBO_LOGGER_WRITER_H
#define TURBO_LOGGER_WRITER_H
#include <iostream>
#include <utility>
#include "turbo_logger.h"

namespace turbo::log {
const int BUFFER_MAX_ITEM = 8192;
const int MAX_FILE_SIZE_IN_MB = 200;
const int ROTATION_FILE_COUNT = 10;

struct LoggerOptions {
    TurboLogLevel minLogLevel = TurboLogLevel::INFO;
    uint32_t maxFileSizeInMB = MAX_FILE_SIZE_IN_MB;    // 日志文件最大大小
    uint32_t rotationFileCount = ROTATION_FILE_COUNT;  // 绕接个数
    uint32_t bufferMaxItem = BUFFER_MAX_ITEM;          // 日志缓冲区最大日志条目
    TurboLogLevel minSyslogLevel = TurboLogLevel::INFO;
    std::string logPath = "/var/log/ubturbo";
};

class RackLoggerWriter {
public:
    virtual ~RackLoggerWriter() = default;
    explicit RackLoggerWriter() = default;

    virtual bool Write(TurboLoggerEntry &loggerEntry) = 0;
};

class RackDefaultLoggerWriter : public RackLoggerWriter {
public:
    explicit RackDefaultLoggerWriter() = default;

    bool Write(TurboLoggerEntry &loggerEntry) override
    {
        loggerEntry.OutPutLog(std::cout);
        return true;
    }
};
}
#endif // TURBO_LOGGER_WRITER_H
