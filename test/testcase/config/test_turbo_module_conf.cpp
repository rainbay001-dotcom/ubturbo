/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "turbo_error.h"

#define private public
#include "turbo_module_conf.h"
#include "turbo_conf_manager.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;
using namespace turbo::config;
using namespace turbo::common;

class TestTurboModuleConf : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(TestTurboModuleConf, MoudleInitFailedTEST)
{
    TurboModuleConf moudleConf;
    MOCKER_CPP(&TurboConfManager::Init, RetCode(*)(void *)).stubs().will(returnValue(TURBO_ERROR));
    RetCode ret = moudleConf.Init();
    EXPECT_EQ(ret, TURBO_ERROR);
}

TEST_F(TestTurboModuleConf, MoudleStartTEST)
{
    TurboModuleConf moudleConf;
    RetCode ret = moudleConf.Start();
    EXPECT_EQ(ret, TURBO_OK);
}

TEST_F(TestTurboModuleConf, MoudleStoptTEST)
{
    TurboModuleConf moudleConf;
    moudleConf.Stop();
}

TEST_F(TestTurboModuleConf, UnInitTEST)
{
    TurboModuleConf moudleConf;
    moudleConf.UnInit();
}

TEST_F(TestTurboModuleConf, NameTEST)
{
    TurboModuleConf moudleConf;
    string ret = moudleConf.Name();
    EXPECT_EQ(ret, "conf");
}