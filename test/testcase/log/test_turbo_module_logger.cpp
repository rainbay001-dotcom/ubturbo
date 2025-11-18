/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_turbo_module_logger.h"
#include "turbo_error.h"

#include <sys/syslog.h>
#include <chrono>
#include <sstream>
#include <thread>

namespace turbo::log {
void TestTurboModuleLogger::SetUp()
{
    Test::SetUp();
}

void TestTurboModuleLogger::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TurboModuleLogger logger;

TEST_F(TestTurboModuleLogger, TestInitFailed)
{
    auto ret = logger.Init();
    EXPECT_EQ(ret, TURBO_ERROR);
}

/*
 * 用例描述：
 * 测试TurboModuleLogger类的Start方法
 */
TEST_F(TestTurboModuleLogger, testStartSuccess)
{
    auto ret = logger.Start();
    EXPECT_EQ(ret, TURBO_OK);
}

/*
 * 用例描述：
 * 测试TurboModuleLogger类的Name方法
 */
TEST_F(TestTurboModuleLogger, testNameSuccess)
{
    std::string ret = logger.Name();
    EXPECT_EQ(ret, "logger");
}
} // namespace turbo::log