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
#ifndef TURBO_LOGGER_MANAGER_H
#define TURBO_LOGGER_MANAGER_H

#include "rack_common_def.h"
#include "rack_logger_ringbuffer.h"
#include "rack_logger_filesink.h"
#include "rack_logger_filter.h"
#include "rack_logger_writer.h"

namespace turbo::log {
using namespace rack::common::def;

class RackLoggerManager {
public:
    static RackLoggerManager *Instance();

    static void Destroy();

    explicit RackLoggerManager() = default;

    ~RackLoggerManager()
    {
        delete writer;
    };

    RackResult Init(const LoggerOptions &options, RackLoggerWriter *logWriter);

    void Exit();

    bool IsLog(TurboLogLevel level);

    void Push(TurboLoggerEntry &&loggerEntry);

    void Pop();
    void LogToSyslog(TurboLoggerEntry &loggerEntry);

    static uint32_t LogToSyslogLevel(TurboLogLevel &level);
    bool IsSysLog(TurboLoggerEntry &rackLoggerEntry);
    void SetLogLevel(TurboLogLevel level);

    static TurboLogLevel StringToLogLevel(const std::string &level);

    static RackLoggerManager *gInstance;

private:
    static bool gInited;
    TurboLogLevel minLogLevel = TurboLogLevel::INFO;
    TurboLogLevel minSyslogLevel = TurboLogLevel::INFO;
    static std::atomic<bool> threadRunning;
    std::unique_ptr<LogBuffer> logBuffer;
    static std::thread loggingThread;
    RackLoggerWriter *writer = nullptr;
    RackLoggerFilter rackLoggerFilter;
};
}

#endif // TURBO_LOGGER_MANAGER_H