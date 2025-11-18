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
#include "turbo_logger.h"

#include <unistd.h>  // for getpid, pid_t
#include <algorithm> // for max
#include <chrono>    // for duration_cast, duration, high_resol...
#include <ctime>     // for time_t, gmtime, strftime
#include <iomanip>   // for operator<<, setfill, setw
#include <iostream>  // for basic_ostream, operator<<, ostream
#include <new>       // for nothrow
#include <utility>   // for move

#include "rack_logger_manager.h" // for RackLoggerManager
#include "securec.h"             // for memcpy_s, EOK, errno_t

namespace turbo::log {

static uint64_t GetTimeStamp()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::high_resolution_clock::now().time_since_epoch())
                                     .count());
}

static pid_t GetProcessId()
{
    static pid_t pid = getpid();
    return pid;
}

static std::thread::id GetThreadId()
{
    static thread_local const std::thread::id tid = std::this_thread::get_id();
    return tid;
}

static void FormatTimestamp(std::ostream &os, uint64_t timestamp)
{
    std::time_t timet = static_cast<std::time_t>(timestamp / 1000000) + 8 * 60 * 60; // 时区8小时为8*60*60秒
    std::tm time;
    gmtime_r(&timet, &time);
    char buffer[32]; // 设置缓冲区大小为32
    size_t len = strftime(buffer, sizeof(buffer), "%Y-%m-%d %T.", &time);
    if (len == 0) {
        std::cerr << "Time string truncated." << std::endl;
        buffer[0] = '\0';
    }
    uint64_t milliseconds = (timestamp % 1000000) / 1000; // timestamp % 1000000提取时间戳微秒部分
    os << '[' << buffer << std::setw(3) << std::setfill('0') << milliseconds << " +0800]"; // 毫秒格式化3位
}

static const char *LogLevelToString(TurboLogLevel level)
{
    switch (level) {
        case TurboLogLevel::DEBUG:
            return "DEBUG";
        case TurboLogLevel::INFO:
            return "INFO";
        case TurboLogLevel::WARN:
            return "WARN";
        case TurboLogLevel::ERROR:
            return "ERROR";
        case TurboLogLevel::CRIT:
            return "CRIT";
        default:
            return "UNKNOWN";
    }
}

TurboLoggerEntry::TurboLoggerEntry()
{
    errno_t ret = memset_s(logEntryBuffer, sizeof(logEntryBuffer), '\0', sizeof(logEntryBuffer));
    if (EOK != ret) {
        heapBuffer.reset();
        std::cerr << "Failed to copy heapBuffer." << std::endl;
    }
};

TurboLoggerEntry::TurboLoggerEntry(const char *gModuleName, TurboLogLevel level, const char *file, const char *func,
                                   uint32_t line)
    : moduleName(gModuleName ? gModuleName : ""),
      level(level),
      file(file),
      func(func),
      line(line),
      maxSize(sizeof(logEntryBuffer)),
      currentSize(0)
{
    errno_t ret = memset_s(logEntryBuffer, sizeof(logEntryBuffer), '\0', sizeof(logEntryBuffer));
    if (EOK != ret) {
        heapBuffer.reset();
        std::cerr << "Failed to copy heapBuffer." << std::endl;
    }
    timeStamp = GetTimeStamp();
    pid = GetProcessId();
    tid = GetThreadId();
}

TurboLoggerEntry::TurboLoggerEntry(const TurboLoggerEntry &other)
    : moduleName(other.moduleName),
      timeStamp(other.timeStamp),
      pid(other.pid),
      tid(other.tid),
      level(other.level),
      file(other.file),
      func(other.func),
      line(other.line),
      maxSize(other.maxSize),
      currentSize(other.currentSize)
{
    errno_t ret = memset_s(logEntryBuffer, sizeof(logEntryBuffer), '\0', sizeof(logEntryBuffer));
    if (EOK != ret) {
        heapBuffer.reset();
        std::cerr << "Failed to copy heapBuffer." << std::endl;
    }
    if (other.heapBuffer) {
        heapBuffer = std::make_unique<char[]>(maxSize);
        ret = memcpy_s(heapBuffer.get(), currentSize, other.heapBuffer.get(), currentSize);
        if (EOK != ret) {
            heapBuffer.reset();
            std::cerr << "Failed to copy heapBuffer." << std::endl;
        }
    } else {
        ret = memcpy_s(logEntryBuffer, sizeof(logEntryBuffer), other.logEntryBuffer, currentSize);
        if (EOK != ret) {
            std::cerr << "Failed to copy logEntryBuffer , ret is " << ret << "." << std::endl;
        }
    }
}

TurboLoggerEntry &TurboLoggerEntry::operator=(const TurboLoggerEntry &other)
{
    if (this == &other) {
        return *this;
    }

    moduleName = other.moduleName;
    timeStamp = other.timeStamp;
    pid = other.pid;
    tid = other.tid;
    level = other.level;
    file = other.file;
    func = other.func;
    line = other.line;
    maxSize = other.maxSize;
    currentSize = other.currentSize;
    if (other.heapBuffer) {
        heapBuffer = std::make_unique<char[]>(maxSize);
        errno_t ret = memcpy_s(heapBuffer.get(), currentSize, other.heapBuffer.get(), currentSize);
        if (EOK != ret) {
            heapBuffer.reset();
            std::cerr << "ERROR: failed to copy." << std::endl;
        }
    } else {
        heapBuffer.reset();
        errno_t ret = memcpy_s(logEntryBuffer, currentSize, other.logEntryBuffer, currentSize);
        if (EOK != ret) {
            std::cerr << "ERROR: failed to copy." << std::endl;
        }
    }

    return *this;
}

void TurboLoggerEntry::OutPutLog(std::ostream &os)
{
    char *start = !heapBuffer ? &logEntryBuffer[0] : &(heapBuffer.get())[0];
    const char *const end = start + currentSize;

    FormatTimestamp(os, timeStamp);
    os << '[' << LogLevelToString(level) << "][" << pid << "][" << tid << "]";
    if (func) {
        os << "[" << file << ':' << func << ':' << line << "] ";
    }
    DecodeData(os, start, end);
    os << std::endl;
}
void TurboLoggerEntry::FormatSyslog(std::ostream &os)
{
    char *start = !heapBuffer ? &logEntryBuffer[0] : &(heapBuffer.get())[0];
    const char *const end = start + currentSize;
    if (func) {
        os << "[" << file << ':' << func << ':' << line << "] ";
    }
    DecodeData(os, start, end);
    os << std::endl;
}
const char *TurboLoggerEntry::GetModuleName()
{
    return moduleName.c_str();
}

const char *TurboLoggerEntry::GetFile()
{
    return file;
}

uint32_t TurboLoggerEntry::GetLine()
{
    return line;
}
const char *TurboLoggerEntry::GetSyslogAsString()
{
    std::ostringstream oss;
    FormatSyslog(oss);
    return oss.str().c_str();
}
TurboLogLevel TurboLoggerEntry::GetLogLevel()
{
    return level;
}
uint64_t TurboLoggerEntry::GetEntryTimeStamp()
{
    return timeStamp;
}

char *TurboLoggerEntry::GetMessage(size_t &length)
{
    length = currentSize;
    return !heapBuffer ? &logEntryBuffer[0] : &(heapBuffer.get())[0];
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(char data)
{
    EncodeData<char>(TurboLoggerTypeId::CHAR, data);
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(int32_t data)
{
    EncodeData<int32_t>(TurboLoggerTypeId::INT32, data);
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(uint32_t data)
{
    EncodeData<uint32_t>(TurboLoggerTypeId::UINT32, data);
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(int64_t data)
{
    EncodeData<int64_t>(TurboLoggerTypeId::INT64, data);
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(uint64_t data)
{
    EncodeData<uint64_t>(TurboLoggerTypeId::UINT64, data);
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(double data)
{
    EncodeData<double>(TurboLoggerTypeId::DOUBLE, data);
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(const std::string &data)
{
    EncodeString(data.c_str(), data.length());
    return *this;
}

void TurboLoggerEntry::ResizeBuffer(size_t addSize)
{
    size_t const newSize = currentSize + addSize;
    if (newSize <= maxSize)
        return;
    if (!heapBuffer) {
        maxSize = std::max(static_cast<size_t>(512), newSize); // 缓冲区大小512
        heapBuffer.reset(new (std::nothrow) char[maxSize]);
        errno_t ret = memcpy_s(heapBuffer.get(), currentSize, logEntryBuffer, currentSize);
        if (ret != EOK) {
            std::cerr << "Failed to copy heapBuffer." << std::endl;
        }
        return;
    } else {
        maxSize = std::max(static_cast<size_t>(2 * maxSize), newSize); // 重新分配2倍大小buffer
        std::unique_ptr<char[]> newHeapBuffer = std::make_unique<char[]>(maxSize);
        errno_t ret = memcpy_s(newHeapBuffer.get(), currentSize, heapBuffer.get(), currentSize);
        if (ret != EOK) {
            std::cerr << "Failed to copy heapBuffer." << std::endl;
        }
        heapBuffer.swap(newHeapBuffer);
    }
}

char *TurboLoggerEntry::GetBuffer()
{
    if (!heapBuffer) {
        return &logEntryBuffer[currentSize];
    }
    return &(heapBuffer.get())[currentSize];
}

void TurboLoggerEntry::EncodeString(const char *data, size_t length)
{
    if (length == 0) {
        return;
    }
    ResizeBuffer(sizeof(TurboLoggerTypeId) + length + 1);
    char *buffer = GetBuffer();
    *reinterpret_cast<TurboLoggerTypeId *>(buffer++) = TurboLoggerTypeId::CHARPTR;

    errno_t ret = memcpy_s(buffer, length + 1, data, length + 1);
    if (ret != EOK) {
        std::cerr << "Failed to copy heapBuffer." << std::endl;
    }
    currentSize += sizeof(TurboLoggerTypeId) + length + 1;
}

void TurboLoggerEntry::EncodeData(const char *data)
{
    if (data != nullptr) {
        EncodeString(data, strlen(data));
    }
}

void TurboLoggerEntry::EncodeData(TurboLoggerString data)
{
    EncodeData<TurboLoggerString>(TurboLoggerTypeId::STRING, data);
}

char *TurboLoggerEntry::DecodeChar(std::ostream &os, char *buffer)
{
    char data = *reinterpret_cast<char *>(buffer);
    os << data;
    return buffer + sizeof(char);
}

char *TurboLoggerEntry::DecodeUint(std::ostream &os, char *buffer)
{
    uint32_t data = *reinterpret_cast<uint32_t *>(buffer);
    os << data;
    return buffer + sizeof(uint32_t);
}

char *TurboLoggerEntry::DecodeUlong(std::ostream &os, char *buffer)
{
    uint64_t data = *reinterpret_cast<uint64_t *>(buffer);
    os << data;
    return buffer + sizeof(uint64_t);
}

char *TurboLoggerEntry::DecodeInt(std::ostream &os, char *buffer)
{
    int32_t data = *reinterpret_cast<int32_t *>(buffer);
    os << data;
    return buffer + sizeof(int32_t);
}

char *TurboLoggerEntry::DecodeLong(std::ostream &os, char *buffer)
{
    int64_t data = *reinterpret_cast<int64_t *>(buffer);
    os << data;
    return buffer + sizeof(int64_t);
}
char *TurboLoggerEntry::DecodeDouble(std::ostream &os, char *buffer)
{
    double data = *reinterpret_cast<double *>(buffer);
    os << data;
    return buffer + sizeof(double);
}

char *TurboLoggerEntry::DecodeString(std::ostream &os, char *buffer)
{
    TurboLoggerString loggerString = *reinterpret_cast<TurboLoggerString *>(buffer);
    os << loggerString.data;
    return buffer + sizeof(TurboLoggerString);
}
char *TurboLoggerEntry::DecodeConstChar(std::ostream &os, char *buffer)
{
    while (*buffer != '\0') {
        os << *buffer;
        ++buffer;
    }
    return ++buffer;
}
void TurboLoggerEntry::DecodeData(std::ostream &os, char *start, const char *end)
{
    while (start < end) {
        auto id = static_cast<TurboLoggerTypeId>(*start);
        start++;
        switch (id) {
            case TurboLoggerTypeId::CHAR:
                start = DecodeChar(os, start);
                break;
            case TurboLoggerTypeId::UINT32:
                start = DecodeUint(os, start);
                break;
            case TurboLoggerTypeId::UINT64:
                start = DecodeUlong(os, start);
                break;
            case TurboLoggerTypeId::INT32:
                start = DecodeInt(os, start);
                break;
            case TurboLoggerTypeId::INT64:
                start = DecodeLong(os, start);
                break;
            case TurboLoggerTypeId::DOUBLE:
                start = DecodeDouble(os, start);
                break;
            case TurboLoggerTypeId::STRING:
                start = DecodeString(os, start);
                break;
            case TurboLoggerTypeId::CHARPTR:
                start = DecodeConstChar(os, start);
                break;
            default:
                return;
        }
    }
}

bool TurboIsLog(TurboLogLevel level)
{
    if (!turbo::log::RackLoggerManager::gInstance) {
        return false;
    }
    return turbo::log::RackLoggerManager::Instance()->IsLog(level);
}

bool TurboLog::operator==(TurboLoggerEntry &loggerEntry)
{
    turbo::log::RackLoggerManager::Instance()->Push(std::move(loggerEntry));
    return true;
}
} // namespace turbo::log