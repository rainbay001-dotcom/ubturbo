/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "test_rack_logger_filter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <chrono>
#include <sstream>
#include <thread>
#include <sys/syslog.h>

#define private public

using namespace ::testing;
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace turbo::log {
void TestRackLoggerFilter::SetUp()
{
    Test::SetUp();
}

void TestRackLoggerFilter::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

constexpr uint16_t FILE_LINE = 10;
/**
 * 用例描述：
 * 设置最大历史记录大小
 * 测试步骤：
 * 1. 设置最大历史记录大小为5
 * 2. 添加6条日志记录
 * 3. 检查历史记录大小
 * 预期结果：
 * 1. SetMaxHistorySize 返回 true
 * 2. 历史记录大小为5
 * 3. 最早的一条记录被删除
 */
TEST_F(TestRackLoggerFilter, SetMaxHistorySize_success)
{
    const size_t maxSize = 5;
    EXPECT_EQ(RACK_OK, filter.SetMaxHistorySize(maxSize));

    for (uint32_t i = 0; i < 6; ++i) { // 添加6条日志记录，大于最大历史记录大小
        TurboLoggerEntry rackLoggerEntry("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE + i);
        rackLoggerEntry << "";
        filter.IsLogFilter(rackLoggerEntry);
    }
    std::ostringstream oss;
    testing::internal::CaptureStdout();
    filter.DisplayFiltersAndHistory();
    std::string output = testing::internal::GetCapturedStdout();

    // 检查输出中包含的缓存条数
    const std::string moduleStr("File:");
    int moduleCount = 0;
    size_t pos = 0;
    while ((pos = output.find(moduleStr, pos)) != std::string::npos) {
        ++moduleCount;
        pos += moduleStr.size(); // "Module:"的长度
    }

    EXPECT_EQ(moduleCount, maxSize);
    EXPECT_EQ(output.find("Module: 0"), std::string::npos);
}

/**
 * 用例描述：
 * 设置过滤周期
 * 测试步骤：
 * 1. 设置过滤周期为2秒
 * 2. 连续发送相同的日志
 * 3. 等待3秒后再次发送相同的日志
 * 预期结果：
 * 1. SetFilterCycle 返回 true
 * 2. 第一条日志通过，第二条被过滤
 * 3. 3秒后的日志通过
 */
TEST_F(TestRackLoggerFilter, SetFilterCycle_success)
{
    const uint32_t cycle = 2;
    EXPECT_EQ(RACK_OK, filter.SetFilterCycle(cycle));
    TurboLoggerEntry rackLoggerEntry1("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry1 << "";

    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry1));
    TurboLoggerEntry rackLoggerEntry2("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry2 << "";
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry2));

    std::this_thread::sleep_for(std::chrono::seconds(3)); // 等待时间超过过滤周期 3seconds
    TurboLoggerEntry rackLoggerEntry3("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry3 << "";
    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry3));
}

/**
 * 用例描述：
 * 显示过滤器状态和历史
 * 测试步骤：
 * 1. 设置一些过滤规则
 * 2. 记录一些日志
 * 3. 调用DisplayFiltersAndHistory方法
 * 预期结果：
 * 1. 输出包含设置的过滤规则信息
 * 2. 输出包含记录的日志历史信息
 */
TEST_F(TestRackLoggerFilter, DisplayFiltersAndHistory_success)
{
    const uint16_t moduleID = 1;
    filter.SetModuleFilterSwitch(moduleID, true);
    filter.SetLevelFilter(moduleID, TurboLogLevel::DEBUG, true);

    TurboLoggerEntry rackLoggerEntry("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry << "";
    filter.IsLogFilter(rackLoggerEntry);
    filter.DisplayFiltersAndHistory();
    testing::internal::CaptureStdout();
    filter.DisplayFiltersAndHistory();
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("Module 1") != std::string::npos);
    EXPECT_TRUE(output.find("Level 0: Filtered") != std::string::npos);
    EXPECT_TRUE(output.find("test.cpp") != std::string::npos);
}

/**
 * 用例描述：
 * 重复日志过滤
 * 测试步骤：
 * 1. 设置过滤周期为2秒
 * 2. 连续发送3条相同的日志
 * 3. 等待3秒后再发送一条相同的日志
 * 预期结果：
 * 1. 第一条日志通过
 * 2. 接下来的2条日志在2秒内被过滤
 * 3. 3秒后的日志通过
 */
TEST_F(TestRackLoggerFilter, DuplicateLogFiltering_success)
{
    const uint32_t cycle = 2;
    const uint16_t moduleID = 1;
    filter.SetFilterCycle(cycle);
    filter.SetModuleFilterSwitch(moduleID, true);

    TurboLoggerEntry rackLoggerEntry1("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry1 << "";
    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry1));
    TurboLoggerEntry rackLoggerEntry2("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry2 << "";
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry2));
    TurboLoggerEntry rackLoggerEntry3("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry3 << "";
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry3));

    std::this_thread::sleep_for(std::chrono::seconds(3)); // 等待时间超过过滤周期3seconds
    TurboLoggerEntry rackLoggerEntry4("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry4 << "";
    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry4));
}

/**
 * 用例描述：
 * 边界条件测试 - 无效输入
 * 测试步骤：
 * 1. 尝试设置无效的最大历史记录大小
 * 2. 尝试设置无效的过滤周期
 * 预期结果：
 * 1. SetMaxHistorySize 返回 false
 * 2. SetFilterCycle 返回 false
 */
TEST_F(TestRackLoggerFilter, InvalidInput)
{
    EXPECT_EQ(RACK_ERROR, filter.SetMaxHistorySize(0));
    EXPECT_EQ(RACK_ERROR, filter.SetFilterCycle(0));
}

/**
 * 用例描述：边界值测试
 * 测试步骤：
 * 1. 测试最小和最大有效模块ID
 * 2. 测试最小和最大有效日志级别
 * 3. 测试最大历史记录大小的边界值
 * 预期结果：所有操作都应正常执行，不会崩溃或产生意外结果
 */
TEST_F(TestRackLoggerFilter, BoundaryValueTest)
{
    filter.SetModuleFilterSwitch(0, true);
    filter.SetModuleFilterSwitch(UINT16_MAX, true);

    const uint16_t moduleID = 1;
    EXPECT_EQ(RACK_OK, filter.SetLevelFilter(moduleID, TurboLogLevel::DEBUG, true));
    EXPECT_EQ(RACK_OK, filter.SetLevelFilter(moduleID, TurboLogLevel::ERROR, true));

    EXPECT_EQ(RACK_OK, filter.SetMaxHistorySize(1));
    EXPECT_EQ(RACK_OK, filter.SetMaxHistorySize(SIZE_MAX));
}

TEST_F(TestRackLoggerFilter, SetLevelFilterTestFailed)
{
    const uint16_t moduleID = 1;
    EXPECT_EQ(RACK_ERROR, filter.SetLevelFilter(moduleID, TurboLogLevel::CRIT, true));
}

TEST_F(TestRackLoggerFilter, ClearFilter_ShouldClearAllInternalState)
{
    const uint32_t cycle = 2;
    EXPECT_EQ(RACK_OK, filter.SetFilterCycle(cycle));
    TurboLoggerEntry rackLoggerEntry1("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry1 << "";

    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry1));
    TurboLoggerEntry rackLoggerEntry2("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry2 << "";
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry2));

    const uint16_t moduleID = 1;
    EXPECT_EQ(RACK_OK, filter.SetLevelFilter(moduleID, TurboLogLevel::DEBUG, true));

    // 确保前置状态不为空
    EXPECT_FALSE(filter.moduleFilters.empty());
    EXPECT_FALSE(filter.filterHistory.empty());

    // 调用清空方法
    filter.ClearFilter();

    // 验证清空后状态
    EXPECT_TRUE(filter.moduleFilters.empty());
    EXPECT_TRUE(filter.filterHistory.empty());
}
/**
 * 用例描述：日志过滤周期边界测试
 * 测试步骤：
 * 1. 设置极短的过滤周期
 * 2. 测试周期刚好结束时的行为
 * 预期结果：系统应正确处理周期设置，并在周期结束时准确允许新日志
 */
TEST_F(TestRackLoggerFilter, FilterCycleBoundaryTest)
{
    const uint16_t moduleID = 1;
    filter.SetModuleFilterSwitch(moduleID, true);

    // 测试极短周期
    const uint32_t shortCycle = 1;
    filter.SetFilterCycle(shortCycle); // 1秒周期
    TurboLoggerEntry rackLoggerEntry1("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry1 << "";
    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry1));
    TurboLoggerEntry rackLoggerEntry2("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry2 << "";
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry2));

    const int milliSecondsPerSecond = 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(shortCycle * milliSecondsPerSecond + 1)); // 略大于1秒
    TurboLoggerEntry rackLoggerEntry3("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry3 << "";
    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry3));
}

/**
 * 用例描述：历史记录溢出测试
 * 测试步骤：
 * 1. 设置一个小的最大历史记录大小
 * 2. 添加超过最大大小的日志记录
 * 3. 检查历史记录的内容
 * 预期结果：应只保留最新的N条记录，其中N为设置的最大大小
 */
TEST_F(TestRackLoggerFilter, HistoryOverflowTest)
{
    const size_t maxHistorySize = 3;
    const size_t logCount = 5;
    const uint16_t moduleID = 1;

    filter.SetMaxHistorySize(maxHistorySize);
    filter.SetModuleFilterSwitch(1, true);

    for (uint32_t i = 0; i < logCount; ++i) {
        TurboLoggerEntry rackLoggerEntry("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", i);
        rackLoggerEntry << "";
        filter.IsLogFilter(rackLoggerEntry);
    }

    testing::internal::CaptureStdout();
    filter.DisplayFiltersAndHistory();
    std::string output = testing::internal::GetCapturedStdout();

    const int stateShowLine = 4;
    EXPECT_EQ(std::count(output.begin(), output.end(), '\n'),
        stateShowLine + maxHistorySize); // 应该只有3行历史记录,状态展示占4行
    EXPECT_TRUE(output.find("Line: 2") != std::string::npos);
    EXPECT_TRUE(output.find("Line: 3") != std::string::npos);
    EXPECT_TRUE(output.find("Line: 4") != std::string::npos);
    EXPECT_TRUE(output.find("Line: 0") == std::string::npos);
    EXPECT_TRUE(output.find("Line: 1") == std::string::npos);
}

/**
 * 用例描述：
 * 重复日志过滤
 * 测试步骤：
 * 1. 设置过滤周期为5秒
 * 2. 交替发送文件名+行号相同，但内容不同的两种日志
 * 预期结果：
 * 1. 两种日志仅第一条日志通过
 */
TEST_F(TestRackLoggerFilter, AlternateDuplicateLogFiltering)
{
    const uint32_t cycle = 5;
    filter.SetFilterCycle(cycle);

    TurboLoggerEntry rackLoggerEntry1("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry1 << "1";
    TurboLoggerEntry rackLoggerEntry2("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry2 << "2";
    TurboLoggerEntry rackLoggerEntry3("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry3 << "1";
    TurboLoggerEntry rackLoggerEntry4("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry4 << "2";
    TurboLoggerEntry rackLoggerEntry5("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry5 << "1";
    TurboLoggerEntry rackLoggerEntry6("testlog", TurboLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    rackLoggerEntry6 << "2";

    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry1));
    EXPECT_FALSE(filter.IsLogFilter(rackLoggerEntry2));
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry3));
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry4));
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry5));
    EXPECT_TRUE(filter.IsLogFilter(rackLoggerEntry6));
}

TEST_F(TestRackLoggerFilter, TestPrintLogInfoFail1)
{
    struct TimeWrapper {
        static std::tm* LocalTime(const std::time_t* t)
        {
            return std::localtime(t);
        }
    };
    LogFilterInfo logFilterInfo;
    MOCKER(TimeWrapper::LocalTime)
        .stubs()
        .with(any())
        .will(returnValue(static_cast<std::tm*>(nullptr)));
    filter.PrintLogInfo(logFilterInfo);
}

TEST_F(TestRackLoggerFilter, TestPrintLogInfoFail2)
{
    LogFilterInfo logFilterInfo;
    logFilterInfo.timestamp = std::chrono::system_clock::now();
    logFilterInfo.rackLoggerEntry.moduleName = "";
    logFilterInfo.rackLoggerEntry.level = TurboLogLevel::INFO;
    logFilterInfo.rackLoggerEntry.file = nullptr;
    logFilterInfo.rackLoggerEntry.func = nullptr;
    logFilterInfo.rackLoggerEntry.line = 0;

    MOCKER_CPP(&TurboLoggerEntry::GetFile, const char* (*)())
        .stubs()
        .will(returnValue((const char*)nullptr));
    filter.PrintLogInfo(logFilterInfo);
}

TEST_F(TestRackLoggerFilter, TestPrintLogInfoSucceed)
{
    LogFilterInfo logFilterInfo;
    logFilterInfo.timestamp = std::chrono::system_clock::now();
    MOCKER_CPP(&TurboLoggerEntry::GetFile, const char* (*)())
        .stubs()
        .will(returnValue((const char*)"mocked_file.cpp"));
    filter.PrintLogInfo(logFilterInfo);
}
} // namespace rack::ut::log
