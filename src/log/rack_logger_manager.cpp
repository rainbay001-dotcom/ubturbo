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
#include "rack_logger_manager.h"
#include <iostream>

#include "rack_error.h"
#include "rack_logger_ringbuffer.h"
#include "sys/syslog.h"

namespace turbo::log {
RackLoggerManager *RackLoggerManager::gInstance = nullptr;
bool RackLoggerManager::gInited = false;
std::atomic<bool> RackLoggerManager::threadRunning;
std::thread RackLoggerManager::loggingThread;

RackLoggerManager *RackLoggerManager::Instance()
{
    /* already created */
    if (gInstance != nullptr) {
        return gInstance;
    }
    gInstance = new (std::nothrow) RackLoggerManager();
    if (gInstance == nullptr) {
        std::cerr << "Failed to new RackLogger object, probably out of memory";
        return nullptr;
    }
    return gInstance;
}

void RackLoggerManager::Destroy()
{
    /* un-initialize and delete logger */
    if (gInstance != nullptr) {
        if (gInited) {
            gInstance->Exit();
        }
        delete gInstance;
        gInstance = nullptr;
    }
}

RackResult RackLoggerManager::Init(const LoggerOptions &options, RackLoggerWriter *logWriter)
{
    if (gInited) {
        return RACK_OK;
    }
    /* create */
    if (logWriter == nullptr) {
        return RACK_ERROR;
    }
    this->minLogLevel = options.minLogLevel;
    this->minSyslogLevel = options.minSyslogLevel;
    gInstance->writer = logWriter;
    threadRunning.store(true);
    try {
        logBuffer = std::make_unique<LogBuffer>(options.bufferMaxItem);
    }catch (const std::bad_alloc&) {
        std::cerr << "Failed to make unique ptr.";
        return RACK_ERROR;
    }

    rackLoggerFilter.SetFilterCycle(5); // 设置过滤周期为5秒
    loggingThread = std::thread([this] { RackLoggerManager::Pop(); });
    gInited = true;
    return RACK_OK;
}

void RackLoggerManager::Exit()
{
    std::unique_lock<std::shared_mutex> lock(logBuffer->mtx);
    logBuffer->stop = true;
    lock.unlock();
    threadRunning.store(false);
    if (loggingThread.joinable()) {
        loggingThread.join();
    }
}

bool RackLoggerManager::IsLog(TurboLogLevel level)
{
    return level >= minLogLevel;
}

void RackLoggerManager::Push(TurboLoggerEntry &&loggerEntry)
{
    if (logBuffer == nullptr) {
        return;
    }
    logBuffer->Push(std::move(loggerEntry));
}

void RackLoggerManager::Pop()
{
    if (logBuffer == nullptr) {
        return;
    }

    TurboLoggerEntry loggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    while (threadRunning.load()) {
        if (logBuffer->Pop(loggerEntry)) {
            if (rackLoggerFilter.IsLogFilter(loggerEntry)) {
                continue;
            }
            writer->Write(loggerEntry);
            bool ret = IsSysLog(loggerEntry);
            if (ret) {
                LogToSyslog(loggerEntry);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(50)); // 环形缓冲区无数据则线程休眠50微秒
        }
    }

    while (logBuffer->Pop(loggerEntry)) {
        if (!rackLoggerFilter.IsLogFilter(loggerEntry)) {
            writer->Write(loggerEntry);
            bool ret = IsSysLog(loggerEntry);
            if (ret) {
                LogToSyslog(loggerEntry);
            }
        }
    }
}
void RackLoggerManager::LogToSyslog(TurboLoggerEntry &loggerEntry)
{
    auto level = loggerEntry.GetLogLevel();
    auto syslogLevel = LogToSyslogLevel(level);
    openlog("ubturbo", 0, 0);
    syslog(syslogLevel, "%s", loggerEntry.GetSyslogAsString());
    closelog();
}
bool RackLoggerManager::IsSysLog(TurboLoggerEntry &rackLoggerEntry)
{
    auto level = rackLoggerEntry.GetLogLevel();
    if (level >= this->minSyslogLevel) {
        return true;
    }
    return false;
}
uint32_t RackLoggerManager::LogToSyslogLevel(TurboLogLevel &level)
{
    if (level == TurboLogLevel::DEBUG) {
        return LOG_DEBUG;
    }
    if (level == TurboLogLevel::INFO) {
        return LOG_INFO;
    }
    if (level == TurboLogLevel::WARN) {
        return LOG_WARNING;
    }
    if (level == TurboLogLevel::ERROR) {
        return LOG_ERR;
    }
    if (level == TurboLogLevel::CRIT) {
        return LOG_CRIT;
    }
    return LOG_INFO;
}
void RackLoggerManager::SetLogLevel(TurboLogLevel level)
{
    minLogLevel = level;
}
TurboLogLevel RackLoggerManager::StringToLogLevel(const std::string &level)
{
    if (level == "DEBUG") {
        return TurboLogLevel::DEBUG;
    }
    if (level == "INFO") {
        return TurboLogLevel::INFO;
    }
    if (level == "WARN") {
        return TurboLogLevel::WARN;
    }
    if (level == "ERROR") {
        return TurboLogLevel::ERROR;
    }
    if (level == "CRIT") {
        return TurboLogLevel::CRIT;
    }
    return TurboLogLevel::INFO;
}
}