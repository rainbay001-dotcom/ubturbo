/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include "test_rack_logger_manager.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "sys/syslog.h"

#define private public
#include "turbo_logger.h"

namespace turbo::log {
void TestRackLoggerManager::SetUp()
{
    Test::SetUp();
    currentPath = std::filesystem::current_path();
}

void TestRackLoggerManager::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestRackLoggerManager, TestInstanceAndDestroy)
{
    LoggerOptions options{TurboLogLevel::INFO, 2, 2, 64, TurboLogLevel::INFO, "/var/log/scbus"};
    RackLoggerWriter *writer = new (std::nothrow) RackDefaultLoggerWriter();
    EXPECT_NE(writer, nullptr);
    auto logManager1 = RackLoggerManager::Instance();
    EXPECT_NE(logManager1, nullptr);
    auto ret = logManager1->Init(options, writer);
    EXPECT_EQ(ret, RACK_OK);
    auto logManager2 = RackLoggerManager::Instance();
    EXPECT_EQ(logManager1, logManager2);
    UBTURBO_LOG_INFO("test", 0) << "first log";
    UBTURBO_LOG_INFO("test", 0) << "second log";
    int sleepTime = 10000;
    usleep(sleepTime);
    RackLoggerManager::Destroy();
}

/*
 * 用例描述：
 * 测试RackLoggerManager类的Init方法
 */
TEST_F(TestRackLoggerManager, TestInit_WhenAlreadyInited)
{
    LoggerOptions options;
    RackLoggerManager rackLoggerManager;
    rackLoggerManager.gInited = true;
    RackLoggerWriter *writer = new (std::nothrow) RackDefaultLoggerWriter();
    EXPECT_EQ(rackLoggerManager.Init(options, writer), RACK_OK);
    delete writer;
    RackLoggerManager::Destroy();
}

TEST_F(TestRackLoggerManager, TestInit_WhenWriterIsNull)
{
    LoggerOptions options;
    RackLoggerManager* logManager = RackLoggerManager::Instance();
    ASSERT_NE(logManager, nullptr);
    RackLoggerManager::gInited = false;
    auto ret = logManager->Init(options, nullptr);
    EXPECT_EQ(ret, RACK_ERROR);
    RackLoggerManager::Destroy();
}

TEST_F(TestRackLoggerManager, TestInit_Normal)
{
    LoggerOptions options{TurboLogLevel::INFO, 2, 2, 64, TurboLogLevel::INFO, "/tmp"};
    RackLoggerManager* logManager = RackLoggerManager::Instance();
    ASSERT_NE(logManager, nullptr);
    RackLoggerWriter *writer = new (std::nothrow) RackDefaultLoggerWriter();
    ASSERT_NE(writer, nullptr);
    auto ret = logManager->Init(options, writer);
    EXPECT_EQ(ret, RACK_OK);
    ret = logManager->Init(options, writer);
    EXPECT_EQ(ret, RACK_OK);
    RackLoggerManager::Destroy();
}

/*
 * 用例描述：
 * 测试RackLoggerManager类的IsLog方法
 */
TEST_F(TestRackLoggerManager, testIsLog)
{
    RackLoggerManager rackLoggerManager;
    TurboLogLevel level = TurboLogLevel::DEBUG;
    rackLoggerManager.SetLogLevel(TurboLogLevel::WARN);
    EXPECT_FALSE(rackLoggerManager.IsLog(level));

    level = TurboLogLevel::INFO;
    EXPECT_FALSE(rackLoggerManager.IsLog(level));

    level = TurboLogLevel::WARN;
    EXPECT_TRUE(rackLoggerManager.IsLog(level));

    level = TurboLogLevel::ERROR;
    EXPECT_TRUE(rackLoggerManager.IsLog(level));

    level = TurboLogLevel::CRIT;
    EXPECT_TRUE(rackLoggerManager.IsLog(level));
}
/*
 * 用例描述：
 * 测试RackLoggerManager类的StringToLogLevel方法
 * 测试步骤：如下
 * 1.设置StringToLogLevel传入的level值为DEBUG
 * 2.设置StringToLogLevel传入的level值为INFO
 * 3.设置StringToLogLevel传入的level值为WARN
 * 4.设置StringToLogLevel传入的level值为ERROR
 * 5.设置StringToLogLevel传入的level值为INVALID
 * 预期结果：如下
 * 1.返回值为RackLogLevel::DEBUG
 * 2.返回值为RackLogLevel::INFO
 * 3.返回值为RackLogLevel::WARN
 * 4.返回值为RackLogLevel::ERROR
 * 5.返回值为RackLogLevel::INFO
 */
TEST_F(TestRackLoggerManager, testStringToLogLevel)
{
    EXPECT_EQ(RackLoggerManager::StringToLogLevel("DEBUG"), TurboLogLevel::DEBUG);
    EXPECT_EQ(RackLoggerManager::StringToLogLevel("INFO"), TurboLogLevel::INFO);
    EXPECT_EQ(RackLoggerManager::StringToLogLevel("WARN"), TurboLogLevel::WARN);
    EXPECT_EQ(RackLoggerManager::StringToLogLevel("ERROR"), TurboLogLevel::ERROR);
    EXPECT_EQ(RackLoggerManager::StringToLogLevel("CRIT"), TurboLogLevel::CRIT);
    EXPECT_EQ(RackLoggerManager::StringToLogLevel("INVALID"), TurboLogLevel::INFO);
}
/*
 * 用例描述：
 * 测试RackLoggerManager类的LogToSyslogLevel方法
 * 测试步骤：如下
 * 1.设置LogToSyslogLevel传入的level值为DEBUG
 * 2.设置LogToSyslogLevel传入的level值为INFO
 * 3.设置LogToSyslogLevel传入的level值为WARN
 * 4.设置LogToSyslogLevel传入的level值为ERROR
 * 5.设置LogToSyslogLevel传入的level值为CRIT
 * 6.设置LogToSyslogLevel传入的level值为UNKNOWN
 * 预期结果：如下
 * 1.返回值为LOG_DEBUG
 * 2.返回值为LOG_INFO
 * 3.返回值为LOG_WARN
 * 4.返回值为LOG_ERROR
 * 5.返回值为LOG_CRIT
 * 6.返回值为LOG_INFO
 */
TEST_F(TestRackLoggerManager, testLogToSyslogLevel)
{
    TurboLogLevel level = TurboLogLevel::DEBUG;
    EXPECT_EQ(RackLoggerManager::LogToSyslogLevel(level), LOG_DEBUG);
    level = TurboLogLevel::INFO;
    EXPECT_EQ(RackLoggerManager::LogToSyslogLevel(level), LOG_INFO);
    level = TurboLogLevel::WARN;
    EXPECT_EQ(RackLoggerManager::LogToSyslogLevel(level), LOG_WARNING);
    level = TurboLogLevel::ERROR;
    EXPECT_EQ(RackLoggerManager::LogToSyslogLevel(level), LOG_ERR);
    level = TurboLogLevel::CRIT;
    EXPECT_EQ(RackLoggerManager::LogToSyslogLevel(level), LOG_CRIT);
}

inline void PushToLogBuffer(LogBuffer *buffer, TurboLoggerEntry &&entry)
{
    buffer->Push(std::move(entry));
}

TEST_F(TestRackLoggerManager, Push)
{
    RackLoggerManager rackLoggerManager;
    uint32_t size = 10;
    rackLoggerManager.logBuffer = std::make_unique<LogBuffer>(size);
    MOCKER(PushToLogBuffer).stubs().will(ignoreReturnValue());
    TurboLoggerEntry loggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    EXPECT_NO_THROW(rackLoggerManager.Push(std::move(loggerEntry)));
}

TEST_F(TestRackLoggerManager, testIsSysLogSuccess)
{
    RackLoggerManager rackLoggerManager;
    TurboLoggerEntry entry("tag", TurboLogLevel::ERROR, "file", "func", 1);
    EXPECT_TRUE(rackLoggerManager.IsSysLog(entry)); // ERROR >= WARN → true
}

TEST_F(TestRackLoggerManager, testIsSysLogFail)
{
    RackLoggerManager rackLoggerManager;
    TurboLoggerEntry entry("tag", TurboLogLevel::DEBUG, "file", "func", 1);
    EXPECT_FALSE(rackLoggerManager.IsSysLog(entry)); // INFO >= DEBUG → false
}

TEST_F(TestRackLoggerManager, testLogToSyslog)
{
    RackLoggerManager rackLoggerManager;
    auto level = TurboLogLevel::INFO;
    TurboLoggerEntry loggerEntry("tag", level, "file", "func", 1);
    MOCKER_CPP(&openlog, void(*)(const char*, int, int))
        .stubs();
    MOCKER_CPP(&syslog, void(*)(int, const char*))
        .stubs();
    MOCKER_CPP(&closelog, void(*)(void))
        .stubs();
    rackLoggerManager.LogToSyslog(loggerEntry);
}

TEST_F(TestRackLoggerManager, PopFail)
{
    RackLoggerManager rackLoggerManager;
    rackLoggerManager.Pop();
}

} // namespace turbo::log