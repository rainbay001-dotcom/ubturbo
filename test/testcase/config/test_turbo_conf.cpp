/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "turbo_conf.h"
#include "turbo_conf_manager.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace turbo::common;
using namespace turbo::config;

class TestConf : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "[Phase SetUp Begin]" << std::endl;
        std::cout << "[Phase SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[Phase TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[Phase TearDown End]" << std::endl;
    }
};

TEST_F(TestConf, UBTurboGetStrTEST)
{
    const std::string section = "a";
    const std::string configKey = "b";
    std::string configVal;
    std::string strVal = "c";
    MOCKER_CPP(&TurboConfManager::GetConf, RetCode(*)(const std::string &, const std::string &, std::string &))
        .stubs()
        .will(returnValue(TURBO_OK));
    RetCode ret = UBTurboGetStr(section, configKey, configVal);
    EXPECT_EQ(ret, TURBO_OK);
}

TEST_F(TestConf, UBTurboGetUInt32TEST)
{
    const std::string section = "a";
    const std::string configKey = "b";
    uint32_t configVal;
    std::string strVal = "1";
    MOCKER_CPP(&TurboConfManager::GetConf, RetCode(*)(const std::string &, const std::string &, std::string &))
        .stubs()
        .with(any(), any(), outBound(strVal))
        .will(returnValue(TURBO_OK));
    RetCode ret = UBTurboGetUInt32(section, configKey, configVal);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestConf, UBTurboGetFloatTEST)
{
    const std::string section = "a";
    const std::string configKey = "b";
    float configVal;
    RetCode ret = UBTurboGetFloat(section, configKey, configVal);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestConf, UBTurboGetBoolTEST)
{
    const std::string section = "a";
    const std::string configKey = "b";
    bool configVal;
    RetCode ret = UBTurboGetBool(section, configKey, configVal);
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestConf, UBTurboGetUInt64TEST)
{
    const std::string section = "a";
    const std::string configKey = "b";
    uint64_t configVal;
    RetCode ret = UBTurboGetUInt64(section, configKey, configVal);
    EXPECT_EQ(ret, TURBO_ERROR);
}