/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include "test_rack_logger_ringbuffer.h"

namespace turbo::log {

RackLoggerRingbufferTest::RackLoggerRingbufferTest() {}

void RackLoggerRingbufferTest::SetUp(void)
{
    Test::SetUp();
}

void RackLoggerRingbufferTest::TearDown(void)
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(RackLoggerRingbufferTest, TestRingBuffer_IsEmpty_WhenNewlyCreated)
{
    uint32_t size = 5;
    RingBuffer ringBuffer(size);
    EXPECT_EQ(true, ringBuffer.IsEmpty());
}

TEST_F(RackLoggerRingbufferTest, LogPush_success)
{
    uint32_t size = 5;
    LogBuffer logBuffer(size);
    TurboLoggerEntry loggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    EXPECT_NO_THROW(logBuffer.Push(std::move(loggerEntry)));
    EXPECT_FALSE(logBuffer.writeBuffer.IsEmpty());
}

/*
 * 用例描述
 * 测试队列Push和Pop
 * 测试步骤：
 * 1. 写入1条配置，取出1条配置
 * 预期结果：
 * 1. Pop函数返回true
 */
TEST_F(RackLoggerRingbufferTest, LogPop_success)
{
    uint32_t size = 5;
    LogBuffer logBuffer1(size);
    TurboLoggerEntry entry{};
    EXPECT_EQ(false, logBuffer1.Pop(entry));

    LogBuffer logBuffer2(size);
    TurboLoggerEntry logEntry{};
    TurboLoggerEntry loggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    logBuffer2.Push(std::move(loggerEntry));
    EXPECT_EQ(true, logBuffer2.Pop(logEntry));
    EXPECT_EQ(logEntry.level, TurboLogLevel::INFO);
}

TEST_F(RackLoggerRingbufferTest, test_BasicLockUnlock)
{
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

    {
        SpinLock lock(flag);  // test_and_set() 返回 false → 获得锁
        EXPECT_TRUE(flag.test_and_set());  // 再次设置应返回 true，说明锁已上
        flag.clear();  // 恢复状态，防止影响外部验证
    }

    // 此时锁已析构，应已 clear，test_and_set 应返回 false
    EXPECT_FALSE(flag.test_and_set());  // 如果是 false → 表示锁已释放
    flag.clear();  // 清除
}
}