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
#include "rack_logger_filter.h"

#include <securec.h>
#include <algorithm>
#include <iostream>
#include <mutex>
#include "rack_logger_manager.h"
#include "turbo_logger.h"

namespace turbo::log {
RackLoggerFilter::RackLoggerFilter(size_t maxHistory) : maxHistorySize(maxHistory), filterCycle(0) {}

RackLoggerFilter::~RackLoggerFilter() {}

RackResult RackLoggerFilter::SetMaxHistorySize(size_t size)
{
    if (size == 0) {
        std::cerr << "HistorySize cannot be 0." << std::endl;
        return RACK_ERROR;
    }
    maxHistorySize.store(size, std::memory_order_relaxed);
    std::unique_lock<std::shared_mutex> lock(mutex);
    while (filterHistory.size() > size) {
        // 找到并删除最旧的记录
        auto oldest = std::min_element(filterHistory.begin(), filterHistory.end(), CompareTimestamp);
        filterHistory.erase(oldest);
    }
    return RACK_OK;
}

RackResult RackLoggerFilter::SetFilterCycle(uint32_t cycle)
{
    if (cycle == 0) {
        std::cerr << "FilterCycle cannot be 0." << std::endl;
        return RACK_ERROR;
    }
    if (cycle == UINT32_MAX) {
        std::cerr << "Invalid FilterCycle." << std::endl;
        return RACK_ERROR;
    }
    filterCycle.store(cycle, std::memory_order_relaxed);
    return RACK_OK;
}

void RackLoggerFilter::SetModuleFilterSwitch(uint16_t moduleId, bool enabled)
{
    std::unique_lock<std::shared_mutex> lock(mutex);
    moduleFilters[moduleId].enabled = enabled;
}

RackResult RackLoggerFilter::SetLevelFilter(uint16_t moduleId, TurboLogLevel level, bool filter)
{
    if (level > TurboLogLevel::ERROR) {
        std::cerr << "Invalid log level." << std::endl;
        return RACK_ERROR;
    }
    std::unique_lock<std::shared_mutex> lock(mutex);
    moduleFilters[moduleId].levelFilters[level] = filter;
    return RACK_OK;
}

void RackLoggerFilter::ClearFilter()
{
    std::unique_lock<std::shared_mutex> lock(mutex);
    moduleFilters.clear();
    filterHistory.clear();
}

void RackLoggerFilter::DisplayLogLevelStatus() const
{
    std::shared_lock<std::shared_mutex> lock(mutex);
    std::cout << "Log Level Status:" << std::endl;
    for (const auto &[moduleId, info] : moduleFilters) {
        std::cout << "Module " << moduleId << " (Enabled: " << (info.enabled ? "Yes" : "No") << "):" << std::endl;
        for (const auto &[level, filtered] : info.levelFilters) {
            std::cout << "  Level " << static_cast<int>(level) << ": " << (filtered ? "Filtered" : "Not Filtered")
                      << std::endl;
        }
    }
}

void RackLoggerFilter::DisplayFiltersAndHistory()
{
    std::shared_lock<std::shared_mutex> lock(mutex);
    DisplayLogLevelStatus();
    std::cout << "\nFilter History:" << std::endl;
    for (auto &[key, info] : filterHistory) {
        PrintLogInfo(info);
    }
}

bool RackLoggerFilter::IsLogFilter(TurboLoggerEntry &rackLoggerEntry)
{
    auto timeStamp = rackLoggerEntry.GetEntryTimeStamp();
    std::chrono::system_clock::time_point curLogTime =
        std::chrono::system_clock::time_point(std::chrono::microseconds(timeStamp));
    LogFilterInfo info{rackLoggerEntry, curLogTime};

    std::chrono::system_clock::time_point lastLogTime;
    if (IsDuplicateLog(info, lastLogTime)) {
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(info.timestamp - lastLogTime);
        if (timeDiff.count() < filterCycle.load(std::memory_order_relaxed)) {
            UpdateFilterHistory(info, true, false); // 不更新时间戳
            return true;                            // 重复日志，被过滤
        }
    }

    UpdateFilterHistory(info, false, true); // 更新时间戳
    return false;                           // 允许日志通过
}

bool RackLoggerFilter::IsDuplicateLog(LogFilterInfo &info, std::chrono::system_clock::time_point &lastLogTime)
{
    LogKey key{info.rackLoggerEntry.GetFile(), info.rackLoggerEntry.GetLine()};
    size_t length{};
    char *buffer = info.rackLoggerEntry.GetMessage(length);
    if (buffer == nullptr) {
        std::cerr << "GetMessage failed" << std::endl;
        return false;
    }
    key.context = std::make_unique<char[]>(length);
    errno_t ret = memcpy_s(key.context.get(), length, buffer, length);
    if (EOK != ret) {
        return false;
    }
    key.length = length;

    auto it = filterHistory.find(key);
    if (it != filterHistory.end()) {
        lastLogTime = it->second.timestamp;
        return true;
    }
    return false;
}

void RackLoggerFilter::UpdateFilterHistory(LogFilterInfo &info, bool filtered, bool updateTimestamp)
{
    LogKey key{info.rackLoggerEntry.GetFile(), info.rackLoggerEntry.GetLine()};
    size_t length{};
    char *buffer = info.rackLoggerEntry.GetMessage(length);
    key.context = std::make_unique<char[]>(length);
    errno_t ret = memcpy_s(key.context.get(), length, buffer, length);
    if (EOK != ret) {
        return;
    }
    key.length = length;

    auto [it, inserted] = filterHistory.try_emplace(std::move(key), info); // 将日志插入历史记录
    if (!inserted) {
        if (updateTimestamp) {
            it->second.timestamp = info.timestamp; // 只在日志通过时更新时间戳
        }
        if (filtered) {
            it->second.filterCount++;
        } else {
            it->second.filterCount = 0; // 重置计数器，如果日志通过
        }
    } else {
        it->second.filterCount = filtered ? 1 : 0;
    }

    // 如果超出最大大小，删除最旧的记录
    while (filterHistory.size() > maxHistorySize.load(std::memory_order_relaxed)) {
        auto oldest = std::min_element(filterHistory.begin(), filterHistory.end(), CompareTimestamp);
        filterHistory.erase(oldest);
    }
}

void RackLoggerFilter::PrintLogInfo(LogFilterInfo &info)
{
    std::time_t timestamp = std::chrono::system_clock::to_time_t(info.timestamp);
    struct tm *timeinfo = std::localtime(&timestamp);
    if (timeinfo == nullptr) {
        std::cerr << "localtime failed" << std::endl;
        return;
    }
    std::stringstream timeStream;
    timeStream << std::put_time(timeinfo, "%F %T");
    if (info.rackLoggerEntry.GetFile() == nullptr) {
        std::cerr << "File not exit" << std::endl;
        return;
    }
    std::cout << "File: " << info.rackLoggerEntry.GetFile() << ", Line: " << info.rackLoggerEntry.GetLine()
              << ", Filter Count: " << info.filterCount << ", Last Log Time: " << timeStream.str() << std::endl;
}

bool RackLoggerFilter::CompareTimestamp(const std::pair<const LogKey, LogFilterInfo> &a,
                                        const std::pair<const LogKey, LogFilterInfo> &b)
{
    return a.second.timestamp < b.second.timestamp;
}
} // namespace turbo::log