/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/
#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include <sys/ioctl.h>
#include <fstream>
#include <vector>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "ucache_driver_interaction.h"
#include "turbo_logger.h"
#include "rmrs_error.h"
#include "rmrs_config.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;
using namespace turbo::log;
using namespace rmrs;
using namespace ucache;

class TestUcacheDriverInteraction : public ::testing::Test {
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


TEST_F(TestUcacheDriverInteraction, MigrateSuccessTest)
{
    ucache::DriverInteraction &driver = ucache::DriverInteraction::GetInstance();
    MOCKER_CPP(&ucache::DriverInteraction::EnsureDevice, bool (*)(void *)).stubs().will(returnValue(true));
    MOCKER_CPP(ioctl, int (*)(int, unsigned long, void *)).stubs().will(returnValue(0)).then(returnValue(1));
    struct MigrateSuccess Arg = {1, 1, 1};
    uint32_t ret = driver.GetMigrateSuccess(Arg);
    EXPECT_EQ(ret, RMRS_OK);
    ret = driver.GetMigrateSuccess(Arg);
    EXPECT_EQ(ret, RMRS_ERROR);
}

TEST_F(TestUcacheDriverInteraction, MigrateIoFailedTest)
{
    ucache::DriverInteraction &driver = ucache::DriverInteraction::GetInstance();
    struct MigrateSuccess Arg = {1, 1, 1};
    uint32_t ret = driver.GetMigrateSuccess(Arg);
    EXPECT_EQ(ret, RMRS_ERROR);

    ret = driver.MigrateFoliosInfo(1, 1, 1);
    EXPECT_EQ(ret, RMRS_ERROR);
}

TEST_F(TestUcacheDriverInteraction, EnsureDevice)
{
    ucache::DriverInteraction &driver = ucache::DriverInteraction::GetInstance();
    MOCKER_CPP(stat, int (*)(void *)).stubs().will(returnValue(0));
    uint32_t ret = driver.EnsureDevice();
    EXPECT_EQ(ret, false);

    MOCKER_CPP(open, int (*)(void *)).stubs().will(returnValue(0));
    ret = driver.EnsureDevice();
    EXPECT_EQ(ret, true);
}


TEST_F(TestUcacheDriverInteraction, MigrateSuccessOpenFailTest)
{
    MOCKER_CPP(&ucache::DriverInteraction::EnsureDevice, bool (*)(void *)).stubs().will(returnValue(false));
    ucache::DriverInteraction &driver = ucache::DriverInteraction::GetInstance();
    struct MigrateSuccess Arg = {1, 1, 1};
    uint32_t ret = driver.GetMigrateSuccess(Arg);
    EXPECT_EQ(ret, RMRS_ERROR);
}
 
TEST_F(TestUcacheDriverInteraction, MigrateFoliosInfoTest)
{
    MOCKER_CPP(&ucache::DriverInteraction::EnsureDevice, bool (*)(void *)).stubs().will(returnValue(true));
    MOCKER_CPP(ioctl, int (*)(int, unsigned long, void *)).stubs().will(returnValue(0));
    ucache::DriverInteraction &driver = ucache::DriverInteraction::GetInstance();
    uint32_t ret = driver.MigrateFoliosInfo(1, 1, 1);
    EXPECT_EQ(ret, RMRS_OK);

    GlobalMockObject::verify();
    MOCKER_CPP(&ucache::DriverInteraction::EnsureDevice, bool (*)(void *)).stubs().will(returnValue(true));
    MOCKER_CPP(ioctl, int (*)(int, unsigned long, void *)).stubs().will(returnValue(-1));
    ret = driver.MigrateFoliosInfo(1, 1, 1);
    EXPECT_EQ(ret, RMRS_ERROR);
}
 
TEST_F(TestUcacheDriverInteraction, MigrateFoliosInfoOpenFailTest)
{
    MOCKER_CPP(&ucache::DriverInteraction::EnsureDevice, bool (*)(void *)).stubs().will(returnValue(false));
    ucache::DriverInteraction &driver = ucache::DriverInteraction::GetInstance();

    uint32_t ret = driver.MigrateFoliosInfo(1, 1, 1);
    EXPECT_EQ(ret, RMRS_ERROR);
}