/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.h>
#include <mockcpp/mokc.h>
#include "turbo_error.h"
#define private public
#include "turbo_module_plugin.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace turbo::plugin {
using namespace turbo::common;
class TestTurboModulePlugin : public ::testing::Test {
protected:
    std::string pluginNameMock = "testPlugin";
    std::string fileNameMock = "testFile.so";
    const uint16_t moduleCodeMock = 888;
    TurboModulePlugin pMgr;
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestTurboModulePlugin, TestInit)
{
    EXPECT_EQ(pMgr.Init(), TURBO_OK);
}

TEST_F(TestTurboModulePlugin, TestStart)
{
    EXPECT_EQ(pMgr.Start(), TURBO_OK);
}

TEST_F(TestTurboModulePlugin, TestStop)
{
    pMgr.Stop();
}

TEST_F(TestTurboModulePlugin, TestName)
{
    EXPECT_EQ(pMgr.Name(), "plugin");
}

TEST_F(TestTurboModulePlugin, TesUnInit)
{
    pMgr.UnInit();
}
} // namespace turbo::plugin