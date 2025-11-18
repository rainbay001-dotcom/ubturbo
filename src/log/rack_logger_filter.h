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
#ifndef TURBO_LOG_FILTER_H
#define TURBO_LOG_FILTER_H

#include <unordered_map>
#include <shared_mutex>
#include <atomic>

#include "rack_common_def.h"
#include "rack_error.h"
#include "turbo_logger.h"

namespace turbo::log {
using namespace rack::common::def;
struct LogFilterInfo {
    TurboLoggerEntry rackLoggerEntry;
    std::chrono::system_clock::time_point timestamp;
    uint32_t filterCount = 0;
};

struct LogKey {
    const std::string fileName;
    uint32_t fileLine;
    std::unique_ptr<char[]> context;
    size_t length;

    bool operator == (const LogKey &other) const
    {
        return fileName == other.fileName && fileLine == other.fileLine && length == other.length &&
            (std::memcmp(context.get(), other.context.get(), length) == 0);
    }
};

struct LogKeyHash {
    std::size_t operator () (const LogKey &k) const
    {
        return (std::hash<std::string>{}(k.fileName)) ^ (std::hash<uint32_t>{}(k.fileLine) << 1) ^
            (std::hash<size_t>{}(k.length) << 2); // 左移2位增加哈希值复杂度
    }
};

class RackLoggerFilter {
public:
    explicit RackLoggerFilter(size_t maxHistory = 20); // 最大历史记录为20
    ~RackLoggerFilter();

    RackLoggerFilter(const RackLoggerFilter &) = delete;
    RackLoggerFilter &operator = (const RackLoggerFilter &) = delete;
    RackLoggerFilter(RackLoggerFilter &&) = delete;
    RackLoggerFilter &operator = (RackLoggerFilter &&) = delete;

    /* *
     * @brief 设置日志过滤器历史记录的最大数量
     * @param[in] size 最大历史记录数量
     */
    RackResult SetMaxHistorySize(size_t size);

    /* *
     * @brief 设置日志过滤周期
     * @param[in] cycle 过滤周期 单位秒
     */
    RackResult SetFilterCycle(uint32_t cycle);

    /* *
     * @brief 设置模块的日志过滤开关
     * @param[in] moduleId 模块ID
     * @param[in] enabled 是否启用过滤
     */
    void SetModuleFilterSwitch(uint16_t moduleId, bool enabled);

    /* *
     * @brief 设置指定模块指定级别的日志过滤
     * @param[in] moduleId 模块ID
     * @param[in] level 日志级别
     * @param[in] filter 是否过滤
     */
    RackResult SetLevelFilter(uint16_t moduleId, TurboLogLevel level, bool filter);

    /* *
     * @brief 清空日志过滤器
     */
    void ClearFilter();

    /* *
     * @brief 显示当前各个日志级别的开关状态
     */
    void DisplayLogLevelStatus() const;

    /* *
     * @brief 显示开启过滤的模块和日志过滤的历史记录
     */
    void DisplayFiltersAndHistory();

    /* *
     * @brief 检查是否应该记录此日志
     * @param[in] info 日志过滤信息
     */
    bool IsLogFilter(TurboLoggerEntry &rackLoggerEntry);

private:
    struct ModuleFilterInfo {
        bool enabled = false;
        std::unordered_map<TurboLogLevel, bool> levelFilters;
    };

    std::unordered_map<uint16_t, ModuleFilterInfo> moduleFilters;
    std::unordered_map<LogKey, LogFilterInfo, LogKeyHash> filterHistory;
    std::atomic<size_t> maxHistorySize;
    std::atomic<int> filterCycle; // in seconds

    mutable std::shared_mutex mutex;

    /* *
     * @brief 检查给定的日志信息是否是重复的
     * @param info 要检查的日志信息
     * @param[out] lastLogTime 如果找到重复日志，存储其最后记录时间
     * @return 如果是重复日志返回 true，否则返回 false
     */
    bool IsDuplicateLog(LogFilterInfo &info, std::chrono::system_clock::time_point &lastLogTime);

    /* *
     * @brief 更新过滤历史记录
     * @param info 要更新的日志信息
     * @param filtered 是否被过滤
     * @param updateTimestamp 是否更新时间戳，默认为 true
     */
    void UpdateFilterHistory(LogFilterInfo &info, bool filtered, bool updateTimestamp = true);

    /* *
     * @brief 打印单条日志信息的详细内容
     * @param info 要打印的日志信息
     */
    void PrintLogInfo(LogFilterInfo &info);

    /* *
     * @brief 比较两个日志记录的时间戳
     * @param a 第一个日志记录
     * @param b 第二个日志记录
     * @return 如果 a 的时间戳早于 b 的时间戳，返回 true
     */
    static bool CompareTimestamp(const std::pair<const LogKey, LogFilterInfo> &a,
        const std::pair<const LogKey, LogFilterInfo> &b);
};
} // namespace turbo::log

#endif // TURBO_LOG_FILTER_H
