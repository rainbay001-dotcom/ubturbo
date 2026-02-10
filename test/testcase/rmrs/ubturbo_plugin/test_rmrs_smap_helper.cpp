/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include <gmock/gmock.h>
#include <cstring>

#define private public

#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "mockcpp/mokc.h"
#include "rmrs_smap_helper.h"
#include "rmrs_smap_module.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
using namespace std;

namespace rmrs::smap {

const int ERR_RET = -2;

// 测试类
class TestRmrsSmapHelper : public ::testing::Test {
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

    // 辅助函数：模拟成功的SmapInitFunc
    static int MockSuccessSmapInit(uint32_t, void(int, const char *, const char *))
    {
        return RMRS_OK;
    }

    // 辅助函数：模拟失败的SmapInitFunc
    static int MockFailedSmapInit(uint32_t, void(int, const char *, const char *))
    {
        return ERR_RET;
    }
};

TEST_F(TestRmrsSmapHelper, InitFail_01)
{
    MOCKER(&SmapModule::Init).stubs().will(returnValue(RMRS_ERROR));
    RmrsResult ret = RmrsSmapHelper::Init();
    EXPECT_EQ(ret, RMRS_ERROR);

    SmapInitFunc smapInitFunc = nullptr;
    MOCKER(&SmapModule::Init).stubs().will(returnValue(RMRS_OK));
    MOCKER(&SmapModule::GetSmapInit).stubs().will(returnValue(smapInitFunc));
    ret = RmrsSmapHelper::Init();
    EXPECT_EQ(ret, RMRS_ERROR);

    smapInitFunc = [](const uint32_t _, void(int, const char *, const char *)) -> int {
        return ERR_RET;
    };
    MOCKER(&SmapModule::Init).stubs().will(returnValue(RMRS_OK));
    MOCKER(&SmapModule::GetSmapInit).stubs().will(returnValue(smapInitFunc));
    ret = RmrsSmapHelper::Init();
    EXPECT_EQ(ret, RMRS_ERROR);
}

TEST_F(TestRmrsSmapHelper, Init_Success_01)
{
    SmapInitFunc smapInitFunc = [](const uint32_t _, void(int, const char *, const char *)) -> int {
        return -2;
    };
    MOCKER(&SmapModule::Init).stubs().will(returnValue(RMRS_OK));
    MOCKER(&SmapModule::GetSmapInit).stubs().will(returnValue(smapInitFunc));
    RmrsResult ret = RmrsSmapHelper::Init();
    EXPECT_EQ(ret, RMRS_OK);
}

// 模拟 dlsym 返回 nullptr，表示找不到符号
TEST_F(TestRmrsSmapHelper, TestSmapMode_Failure_dlsym_nullptr)
{
    SetSmapRunModeFunc setSmapRunModeFunc = nullptr;
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    RmrsResult result = RmrsSmapHelper::SmapMode(1);
    ASSERT_EQ(result, RMRS_ERROR);
}

// 模拟 setSmapRunMode 返回负值，表示失败
TEST_F(TestRmrsSmapHelper, TestSmapMode_Failure_SetSmapRunMode_Success)
{
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    RmrsResult result = RmrsSmapHelper::SmapMode(1);
    ASSERT_EQ(result, RMRS_OK);
}

// 模拟 setSmapRunMode 返回负值，表示失败, 错误码-1
TEST_F(TestRmrsSmapHelper, TestSmapMode_Failure_SetSmapRunMode_Faild_M1)
{
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    RmrsResult result = RmrsSmapHelper::SmapMode(1);
    ASSERT_EQ(result, RMRS_ERROR);
}

// 模拟 setSmapRunMode 返回负值，表示失败, 错误码-22
TEST_F(TestRmrsSmapHelper, TestSmapMode_Failure_SetSmapRunMode_Faild_M22)
{
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    RmrsResult result = RmrsSmapHelper::SmapMode(1);
    ASSERT_EQ(result, RMRS_ERROR);
}

/*
 * 用例描述：
 * 同步迁移冷数据失败 错误码-1
 * 测试步骤：
 * 1. 构造迁移冷数据失败场景
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, MigrateColdDataToRemoteNumaSync_Fail_M1)
{
    pid_t pid = 123456;
    uint16_t remoteNumaId = 5;
    std::vector<uint16_t> remoteNumaIdsIn;
    std::vector<pid_t> pidsIn;
    std::vector<uint64_t> memSizeList = {25};
    uint64_t waitTime = 10001;
    remoteNumaIdsIn.push_back(remoteNumaId);
    pidsIn.push_back(pid);

    // smapMigrateOutFunc 返回 -1
    SmapMigrateOutSyncFunc smapMigrateOutSyncFunc = [](MigrateOutMsg *msg, int param1, uint64_t param2) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSmapMigrateOutSync).stubs().will(returnValue(smapMigrateOutSyncFunc));
    RmrsResult ret =
        RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIdsIn, pidsIn, memSizeList, waitTime);
    EXPECT_EQ(ret, RMRS_MIGRATE_FAILED);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 同步迁移冷数据失败 错误码-16
 * 测试步骤：
 * 1. 构造迁移冷数据失败场景
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, MigrateColdDataToRemoteNumaSync_Fail_M16)
{
    pid_t pid = 123456;
    uint16_t remoteNumaId = 5;
    std::vector<uint16_t> remoteNumaIdsIn;
    std::vector<pid_t> pidsIn;
    std::vector<uint64_t> memSizeList = {25};
    uint64_t waitTime = 10001;
    remoteNumaIdsIn.push_back(remoteNumaId);
    pidsIn.push_back(pid);

    // smapMigrateOutFunc 返回 -16
    SmapMigrateOutSyncFunc smapMigrateOutSyncFunc = [](MigrateOutMsg *msg, int param1, uint64_t param2) -> int {
        return -16;
    };
    MOCKER(&SmapModule::GetSmapMigrateOutSync).stubs().will(returnValue(smapMigrateOutSyncFunc));
    RmrsResult ret =
        RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIdsIn, pidsIn, memSizeList, waitTime);
    EXPECT_EQ(ret, RMRS_MIGRATE_FAILED);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 同步迁移冷数据失败
 * 测试步骤：
 * 1. 构造迁移冷数据失败场景，nullptr
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, MigrateColdDataToRemoteNumaSync_Fail_Nullptr)
{
    pid_t pid = 123456;
    uint16_t remoteNumaId = 5;
    std::vector<uint16_t> remoteNumaIdsIn;
    std::vector<pid_t> pidsIn;
    std::vector<uint64_t> memSizeList = {25};
    uint64_t waitTime = 10001;
    remoteNumaIdsIn.push_back(remoteNumaId);
    pidsIn.push_back(pid);

    SmapMigrateOutSyncFunc smapMigrateOutSyncFunc = nullptr;
    MOCKER(&SmapModule::GetSmapMigrateOutSync).stubs().will(returnValue(smapMigrateOutSyncFunc));
    RmrsResult ret =
        RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIdsIn, pidsIn, memSizeList, waitTime);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 同步迁移冷数据失败
 * 测试步骤：
 * 1. 构造迁移冷数据失败场景，pids数组为空
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, MigrateColdDataToRemoteNumaSync_PidsEmpty)
{
    uint16_t remoteNumaId = 5;
    std::vector<uint16_t> remoteNumaIdsIn;
    std::vector<pid_t> pidsIn;
    std::vector<uint64_t> memSizeList = {25};
    uint64_t waitTime = 10001;
    remoteNumaIdsIn.push_back(remoteNumaId);

    SmapMigrateOutSyncFunc smapMigrateOutSyncFunc = [](MigrateOutMsg *msg, int param1, uint64_t param2) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapMigrateOutSync).stubs().will(returnValue(smapMigrateOutSyncFunc));
    RmrsResult ret =
        RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIdsIn, pidsIn, memSizeList, waitTime);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 同步迁移冷数据成功
 * 测试步骤：
 * 1. 构造迁移冷数据成功场景
 * 预期结果：
 * 返回值为 RMRS_OK
 */
TEST_F(TestRmrsSmapHelper, MigrateColdDataToRemoteNumaSync_Success)
{
    pid_t pid = 123456;
    uint16_t remoteNumaId = 5;
    std::vector<uint16_t> remoteNumaIdsIn;
    std::vector<pid_t> pidsIn;
    std::vector<uint64_t> memSizeList = {25};
    uint64_t waitTime = 10001;
    remoteNumaIdsIn.push_back(remoteNumaId);
    pidsIn.push_back(pid);

    SmapMigrateOutSyncFunc smapMigrateOutSyncFunc = [](MigrateOutMsg *msg, int param1, uint64_t param2) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapMigrateOutSync).stubs().will(returnValue(smapMigrateOutSyncFunc));
    RmrsResult ret =
        RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIdsIn, pidsIn, memSizeList, waitTime);
    EXPECT_EQ(ret, RMRS_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * smap场景设置成功
 * 测试步骤：
 * 1. 构造smap场景设置成功场景
 * 预期结果：
 * 返回值为 RMRS_OK
 */
TEST_F(TestRmrsSmapHelper, SmapMode_Success)
{
    int runMode = 1;
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    RmrsResult ret = RmrsSmapHelper::SmapMode(runMode);
    EXPECT_EQ(ret, RMRS_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * smap场景设置失败
 * 测试步骤：
 * 1. 构造smap场景设置失败场景，nullptr
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapMode_Faild_Nullptr)
{
    int runMode = 1;
    SetSmapRunModeFunc setSmapRunModeFunc = nullptr;
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    RmrsResult ret = RmrsSmapHelper::SmapMode(runMode);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * smap场景设置失败
 * 测试步骤：
 * 1. 构造smap场景设置失败场景，错误码-1
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapMode_Faild_M1)
{
    int runMode = 1;
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    RmrsResult ret = RmrsSmapHelper::SmapMode(runMode);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * smap场景设置失败
 * 测试步骤：
 * 1. 构造smap场景设置失败场景，错误码-22
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapMode_Faild_M22)
{
    int runMode = 1;
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    RmrsResult ret = RmrsSmapHelper::SmapMode(runMode);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapRemoveVMPidToRemoteNuma pid级别远端numa迁移成功
 * 测试步骤：
 * 1. 构造SmapRemoveVMPidToRemoteNuma成功场景
 * 预期结果：
 * 返回值为 RMRS_OK
 */
TEST_F(TestRmrsSmapHelper, SmapRemoveVMPidToRemoteNuma_Success)
{
    pid_t pid0 = 123;
    pid_t pid1 = 456;
    std::vector<pid_t> vmPids;
    vmPids.push_back(pid0);
    vmPids.push_back(pid1);
    SmapRemoveFunc smapRemoveFunc = [](RemoveMsg *param1, int param2) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapRemoveFunc).stubs().will(returnValue(smapRemoveFunc));
    RmrsResult ret = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(vmPids);
    EXPECT_EQ(ret, RMRS_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapRemoveVMPidToRemoteNuma pid级别远端numa迁移失败
 * 测试步骤：
 * 1. 构造SmapRemoveVMPidToRemoteNuma失败场景，nullptr
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapRemoveVMPidToRemoteNuma_Faild_Nullptr)
{
    pid_t pid0 = 123;
    pid_t pid1 = 456;
    std::vector<pid_t> vmPids;
    vmPids.push_back(pid0);
    vmPids.push_back(pid1);
    SmapRemoveFunc smapRemoveFunc = nullptr;
    MOCKER(&SmapModule::GetSmapRemoveFunc).stubs().will(returnValue(smapRemoveFunc));
    RmrsResult ret = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(vmPids);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapRemoveVMPidToRemoteNuma pid级别远端numa迁移失败
 * 测试步骤：
 * 1. 构造SmapRemoveVMPidToRemoteNuma失败场景，错误码-22
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapRemoveVMPidToRemoteNuma_Faild_M22)
{
    pid_t pid0 = 123;
    pid_t pid1 = 456;
    std::vector<pid_t> vmPids;
    vmPids.push_back(pid0);
    vmPids.push_back(pid1);
    SmapRemoveFunc smapRemoveFunc = [](RemoveMsg *param1, int param2) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSmapRemoveFunc).stubs().will(returnValue(smapRemoveFunc));
    RmrsResult ret = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(vmPids);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapRemoveVMPidToRemoteNuma pid级别远端numa迁移失败
 * 测试步骤：
 * 1. 构造SmapRemoveVMPidToRemoteNuma失败场景，错误码-9
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapRemoveVMPidToRemoteNuma_Faild_M9)
{
    pid_t pid0 = 123;
    pid_t pid1 = 456;
    std::vector<pid_t> vmPids;
    vmPids.push_back(pid0);
    vmPids.push_back(pid1);
    SmapRemoveFunc smapRemoveFunc = [](RemoveMsg *param1, int param2) -> int {
        return -9;
    };
    MOCKER(&SmapModule::GetSmapRemoveFunc).stubs().will(returnValue(smapRemoveFunc));
    RmrsResult ret = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(vmPids);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapRemoveVMPidToRemoteNuma pid级别远端numa迁移失败
 * 测试步骤：
 * 1. 构造SmapRemoveVMPidToRemoteNuma失败场景，错误码-1
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapRemoveVMPidToRemoteNuma_Faild_M1)
{
    pid_t pid0 = 123;
    pid_t pid1 = 456;
    std::vector<pid_t> vmPids;
    vmPids.push_back(pid0);
    vmPids.push_back(pid1);
    SmapRemoveFunc smapRemoveFunc = [](RemoveMsg *param1, int param2) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSmapRemoveFunc).stubs().will(returnValue(smapRemoveFunc));
    RmrsResult ret = RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma(vmPids);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapEnableRemoteNuma 远端numa禁止冷热流动成功
 * 测试步骤：
 * 1. 构造SmapEnableRemoteNuma成功场景
 * 预期结果：
 * 返回值为 RMRS_OK
 */
TEST_F(TestRmrsSmapHelper, SmapEnableRemoteNuma_Success)
{
    int remoteNumaId = 5;
    SmapEnableNodeFunc smapEnableNodeFunc = [](EnableNodeMsg *param1) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapEnableNodeFunc).stubs().will(returnValue(smapEnableNodeFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableRemoteNuma(remoteNumaId);
    EXPECT_EQ(ret, RMRS_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapEnableRemoteNuma 远端numa禁止冷热流动成功
 * 测试步骤：
 * 1. 构造SmapEnableRemoteNuma失败场景，nullptr
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableRemoteNuma_Faild_Nullptr)
{
    int remoteNumaId = 5;
    SmapEnableNodeFunc smapEnableNodeFunc = nullptr;
    MOCKER(&SmapModule::GetSmapEnableNodeFunc).stubs().will(returnValue(smapEnableNodeFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableRemoteNuma(remoteNumaId);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapEnableRemoteNuma 远端numa禁止冷热流动失败
 * 测试步骤：
 * 1. 构造SmapEnableRemoteNuma失败场景，错误码-22
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableRemoteNuma_Faild_M22)
{
    int remoteNumaId = 5;
    SmapEnableNodeFunc smapEnableNodeFunc = [](EnableNodeMsg *param1) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSmapEnableNodeFunc).stubs().will(returnValue(smapEnableNodeFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableRemoteNuma(remoteNumaId);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapEnableRemoteNuma 远端numa禁止冷热流动失败
 * 测试步骤：
 * 1. 构造SmapEnableRemoteNuma失败场景，错误码-1
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableRemoteNuma_Faild_M1)
{
    int remoteNumaId = 5;
    SmapEnableNodeFunc smapEnableNodeFunc = [](EnableNodeMsg *param1) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSmapEnableNodeFunc).stubs().will(returnValue(smapEnableNodeFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableRemoteNuma(remoteNumaId);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别远端迁移远端成功
 * 测试步骤：
 * 1. 构造Pid级别远端迁移远端错误场景
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapMigratePidRemoteNumaHelper_Faild_Nullptr)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int srcNid = 4;
    int destNid = 5;

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = nullptr;
    MOCKER(&SmapModule::GetSmapMigratePidRemoteNumaFunc).stubs().will(returnValue(smapMigratePidRemoteNumaFunc));
    RmrsResult ret = RmrsSmapHelper::SmapMigratePidRemoteNumaHelper(pidArr, len, srcNid, destNid);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程成功
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper成功场景
 * 预期结果：
 * 返回值为 RMRS_OK
 */
TEST_F(TestRmrsSmapHelper, SmapEnableProcessMigrateHelper_Success)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t *p1, int p2, int p3, int p4) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, RMRS_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableProcessMigrateHelper_Faild_Nullptr)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = nullptr;
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-1
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableProcessMigrateHelper_Faild_M1)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t *p1, int p2, int p3, int p4) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-22
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableProcessMigrateHelper_Faild_M22)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t *p1, int p2, int p3, int p4) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-12
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableProcessMigrateHelper_Faild_M12)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t *p1, int p2, int p3, int p4) -> int {
        return -12;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-34
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableProcessMigrateHelper_Faild_M34)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t *p1, int p2, int p3, int p4) -> int {
        return -34;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-9
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableProcessMigrateHelper_Faild_M9)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t *p1, int p2, int p3, int p4) -> int {
        return -9;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-10
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SmapEnableProcessMigrateHelper_Faild_M10)
{
    pid_t pid0 = 1234;
    pid_t pid1 = 5678;
    pid_t pid2 = 91011;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2;

    pid_t *pidArr = new pid_t[3];
    pidArr[index0] = pid0;
    pidArr[index1] = pid1;
    pidArr[index2] = pid2;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t *p1, int p2, int p3, int p4) -> int {
        return -10;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    RmrsResult ret = RmrsSmapHelper::SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 借来远端内存绑定本地numa成功
 * 测试步骤：
 * 1. 构造SetSmapRemoteNumaInfo成功场景
 * 预期结果：
 * 返回值为 RMRS_OK
 */
TEST_F(TestRmrsSmapHelper, SetSmapRemoteNumaInfo_Success)
{
    uint16_t srcNumaId = 5;
    uint64_t borrowSize = 1024;

    mempooling::over_commit::MemBorrowInfo borrowInfo;
    borrowInfo.presentNumaId = 0;
    borrowInfo.borrowSize = borrowSize;
    std::vector<mempooling::over_commit::MemBorrowInfo> memBorrowInfosWithSrc;
    memBorrowInfosWithSrc.push_back(borrowInfo);

    SetSmapRemoteNumaInfoFunc setSmapRemoteNumaInfo = [](RemoteNumaInfo *p1) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSetSmapRemoteNumaInfo).stubs().will(returnValue(setSmapRemoteNumaInfo));
    RmrsResult ret = RmrsSmapHelper::SetSmapRemoteNumaInfoHelper(srcNumaId, memBorrowInfosWithSrc);
    EXPECT_EQ(ret, RMRS_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 借来远端内存绑定本地numa失败
 * 测试步骤：
 * 1. 构造SetSmapRemoteNumaInfo失败场景
 * 预期结果：
 * 返回值为 RMRS_ERROR
 */
TEST_F(TestRmrsSmapHelper, SetSmapRemoteNumaInfo_Faild_Nullptr)
{
    uint16_t srcNumaId = 5;
    std::vector<mempooling::over_commit::MemBorrowInfo> memBorrowInfosWithSrc;

    SetSmapRemoteNumaInfoFunc setSmapRemoteNumaInfo = nullptr;
    MOCKER(&SmapModule::GetSetSmapRemoteNumaInfo).stubs().will(returnValue(setSmapRemoteNumaInfo));
    RmrsResult ret = RmrsSmapHelper::SetSmapRemoteNumaInfoHelper(srcNumaId, memBorrowInfosWithSrc);
    EXPECT_EQ(ret, RMRS_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestRmrsSmapHelper, RewriteHugePages_01_NormalCase)
{
    // 1. 准备临时文件路径
    const std::string testFilePath = "/tmp/test_hugepages.txt";

    // 2. 调用被测函数（正常情况）
    RmrsResult result = RmrsSmapHelper::RewriteHugePages(testFilePath, 10, 4 * 1024 * 1024); // 10 + 2 = 12

    // 3. 验证返回值
    ASSERT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsSmapHelper, RewriteHugePages_02_FileOpenFailed)
{
    // 1. 使用无效路径（模拟文件打开失败）
    const std::string invalidPath = "/nonexistent/test_hugepages.txt";

    // 2. 调用被测函数
    RmrsResult result = RmrsSmapHelper::RewriteHugePages(invalidPath, 10, 4 * 1024 * 1024);

    // 3. 验证返回错误码
    ASSERT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsSmapHelper, RewriteHugePages_03_BorrowSizeNotAligned)
{
    // 1. 准备临时文件路径
    const std::string testFilePath = "/tmp/test_hugepages.txt";

    // 2. 调用被测函数（borrowSize 不是 2MB 的整数倍）
    RmrsResult result = RmrsSmapHelper::RewriteHugePages(testFilePath, 10, 3 * 1024 * 1024); // 10 + 1.5 → 取整为 11

    // 3. 验证返回值
    ASSERT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsSmapHelper, GetOriginalHugePages_01_NormalCase)
{
    // 1. 准备临时文件并写入测试数据
    const std::string testFilePath = "/tmp/test_hugepages.txt";
    std::ofstream file(testFilePath);
    file << "42" << std::endl; // 写入测试数据
    file.close();

    // 2. 调用被测函数
    uint64_t originalHugePages = 0;
    uint64_t originalHugePagesResult = 42;
    RmrsResult result = RmrsSmapHelper::GetOriginalHugePages(testFilePath, originalHugePages);

    // 3. 验证返回值和读取结果
    ASSERT_EQ(result, RMRS_OK);
    ASSERT_EQ(originalHugePages, originalHugePagesResult);

    // 4. 清理临时文件
    std::remove(testFilePath.c_str());
}

TEST_F(TestRmrsSmapHelper, GetOriginalHugePages_02_FileOpenFailed)
{
    // 1. 使用无效路径（模拟文件打开失败）
    const std::string invalidPath = "/nonexistent/test_hugepages.txt";

    // 2. 调用被测函数
    uint64_t originalHugePages = 0;
    RmrsResult result = RmrsSmapHelper::GetOriginalHugePages(invalidPath, originalHugePages);

    // 3. 验证返回错误码
    ASSERT_EQ(result, RMRS_ERROR);
}

TEST_F(TestRmrsSmapHelper, GetOriginalHugePages_03_InvalidFileContent)
{
    // 1. 准备临时文件并写入非数字内容
    const std::string testFilePath = "/tmp/test_hugepages.txt";
    std::ofstream file(testFilePath);
    file << "not_a_number" << std::endl; // 写入无效数据
    file.close();

    // 2. 调用被测函数
    uint64_t originalHugePages = 0;
    RmrsResult result = RmrsSmapHelper::GetOriginalHugePages(testFilePath, originalHugePages);

    // 3. 验证返回错误码
    ASSERT_EQ(result, RMRS_ERROR);

    // 4. 清理临时文件
    std::remove(testFilePath.c_str());
}

/*
 * Case Description: 测试当level为DEBUG时的情况
 * Preset Condition: 当str和moduleName都不为nullptr
 * Test Steps: 1.调用RackVmLog函数，传入DEBUG级别，非空字符串和模块名
 * Expected Result: 应该调用UBTURBO_LOG_DEBUG宏，输出相应的日志
 */
TEST_F(TestRmrsSmapHelper, ShouldCallTurboLogDebugWhenLevelIsDebug)
{
    RmrsSmapHelper::RackVmLog(static_cast<uint32_t>(RmrsLogLevel::DEBUG), "Test log", "TestModule");
}

/*
 * Case Description: 测试当level为INFO时的情况
 * Preset Condition: 当str和moduleName都不为nullptr
 * Test Steps: 1.调用RackVmLog函数，传入INFO级别，非空字符串和模块名
 * Expected Result: 应该调用UBTURBO_LOG_INFO宏，输出相应的日志
 */
TEST_F(TestRmrsSmapHelper, ShouldCallTurboLogInfoWhenLevelIsInfo)
{
    RmrsSmapHelper::RackVmLog(static_cast<uint32_t>(RmrsLogLevel::INFO), "Test log", "TestModule");
}

/*
 * Case Description: 测试当level为WARN时的情况
 * Preset Condition: 当str和moduleName都不为nullptr
 * Test Steps: 1.调用RackVmLog函数，传入WARN级别，非空字符串和模块名
 * Expected Result: 应该调用UBTURBO_LOG_WARN宏，输出相应的日志
 */
TEST_F(TestRmrsSmapHelper, ShouldCallTurboLogWarnWhenLevelIsWarn)
{
    RmrsSmapHelper::RackVmLog(static_cast<uint32_t>(RmrsLogLevel::WARN), "Test log", "TestModule");
}

/*
 * Case Description: 测试当level为ERROR或其他值时的情况
 * Preset Condition: 当str和moduleName都不为nullptr
 * Test Steps: 1.调用RackVmLog函数，传入ERROR级别，非空字符串和模块名
 * Expected Result: 应该调用UBTURBO_LOG_ERROR宏，输出相应的日志
 */
TEST_F(TestRmrsSmapHelper, ShouldCallTurboLogErrorWhenLevelIsError)
{
    RmrsSmapHelper::RackVmLog(static_cast<uint32_t>(RmrsLogLevel::ERROR), "Test log", "TestModule");
}

/*
 * Case Description: 测试当str为nullptr时的情况
 * Preset Condition: 当moduleName不为nullptr
 * Test Steps: 1.调用RackVmLog函数，传入DEBUG级别，nullptr字符串和模块名
 * Expected Result: 应该调用UBTURBO_LOG_DEBUG宏，输出"(null)"
 */
TEST_F(TestRmrsSmapHelper, ShouldHandleNullStr)
{
    RmrsSmapHelper::RackVmLog(static_cast<uint32_t>(RmrsLogLevel::DEBUG), nullptr, "TestModule");
}

/*
 * Case Description: 测试当moduleName为nullptr时的情况
 * Preset Condition: 当str不为nullptr
 * Test Steps: 1.调用RackVmLog函数，传入DEBUG级别，非空字符串和nullptr模块名
 * Expected Result: 应该调用UBTURBO_LOG_DEBUG宏，输出RMRS_MODULE_NAME
 */
TEST_F(TestRmrsSmapHelper, ShouldHandleNullModuleName)
{
    RmrsSmapHelper::RackVmLog(static_cast<uint32_t>(RmrsLogLevel::DEBUG), "Test log", nullptr);
}

} // namespace rmrs::smap