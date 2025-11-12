/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: smap5.0 user inner interface ut code
 */

#include <cstdlib>
#include <sys/time.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "manage/manage.h"
#include "smap_inner_interface.h"

const double TEST_DEFAULT_LOCAL_MEM_RATIO = 75.0;

using namespace std;
extern "C" EnvAtomic g_status;
extern "C" struct ProcessManager g_processManager;

class InnerInterfaceTest : public ::testing::Test {
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

TEST_F(InnerInterfaceTest, TestNullptrError)
{
    int ret;
    EnvAtomicSet(&g_status, 1);

    ret = SmapQueryVmMemRatio(nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" void SetAdaptMem(bool flag);
TEST_F(InnerInterfaceTest, TestSmapEnableAdaptMemOne)
{
    int ret;
    MOCKER(SetAdaptMem).stubs();
    ret = SmapEnableAdaptMem(0);
    EXPECT_EQ(0, ret);
}

TEST_F(InnerInterfaceTest, TestSmapEnableAdaptMemTwo)
{
    int ret;
    MOCKER(SetAdaptMem).stubs();
    ret = SmapEnableAdaptMem(1);
    EXPECT_EQ(0, ret);
}

TEST_F(InnerInterfaceTest, TestSmapEnableAdaptMemThree)
{
    int ret;
    MOCKER(SetAdaptMem).stubs();
    ret = SmapEnableAdaptMem(-1);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(InnerInterfaceTest, TestSmapQueryVmMemRatio)
{
    EnvAtomicSet(&g_status, 0);
    int ret = SmapQueryVmMemRatio(nullptr);
    EXPECT_EQ(-EPERM, ret);

    EnvAtomicSet(&g_status, 1);
    struct VmRatioMsg msg;
    struct ProcessManager *manager = GetProcessManager();
    ProcessAttr current;
    current.numaAttr.numaNodes = 0b00010001;
    current.next = nullptr;
    current.type = VM_TYPE;
    current.pid = 1;
    current.strategyAttr.l3RemoteMemRatio[0][0] = TEST_DEFAULT_LOCAL_MEM_RATIO;
    manager->processes = &current;
    ret = SmapQueryVmMemRatio(&msg);
    EXPECT_EQ(1, msg.vr[0].pid);
    EXPECT_EQ(HUNDRED - TEST_DEFAULT_LOCAL_MEM_RATIO, msg.vr[0].ratio);
}