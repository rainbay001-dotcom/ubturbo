/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include <gmock/gmock.h>
#include <cstring>
#include "/usr/include/dlfcn.h"

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "rmrs_smap_helper.h"
#include "rmrs_smap_module.h"

#include <iostream>

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;

namespace rmrs::smap {

// 测试类
class TestRmrsSmapModule : public ::testing::Test {
protected:
    void SetUp() override
    try {
        cout << "[Phase SetUp Begin]" << endl;
        SmapModule::Init();
        cout << "[Phase SetUp End]" << endl;
    } catch (const std::exception& e) {
        cerr << "SetUp failed: " << e.what() << endl;
        throw;  // 重新抛出异常，确保测试失败
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(TestRmrsSmapModule, GetSmapQueryVmFreq_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapQueryVmFreqFunc result = SmapModule::GetSmapQueryVmFreq();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestRmrsSmapModule, GetSetSmapRunModeFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SetSmapRunModeFunc result = SmapModule::GetSetSmapRunModeFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestRmrsSmapModule, GetSmapEnableNodeFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapEnableNodeFunc result = SmapModule::GetSmapEnableNodeFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestRmrsSmapModule, GetSmapEnableProcessMigrateFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapEnableProcessMigrateFunc result = SmapModule::GetSmapEnableProcessMigrateFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestRmrsSmapModule, GetSmapInit_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapInitFunc result = SmapModule::GetSmapInit();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestRmrsSmapModule, GetSetSmapRemoteNumaInfo_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SetSmapRemoteNumaInfoFunc result = SmapModule::GetSetSmapRemoteNumaInfo();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestRmrsSmapModule, GetSmapMigrateOut_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapMigrateOutFunc  result = SmapModule::GetSmapMigrateOut();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestRmrsSmapModule, GetSmapMigrateOutSync_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapMigrateOutSyncFunc result = SmapModule::GetSmapMigrateOutSync();
    ASSERT_EQ(result, nullptr);
}

} // rmrs::smap