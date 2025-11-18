/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <filesystem>
#include <fstream>

#include <dirent.h>
#include <gmock/gmock.h>
#include <cstring>
#include <iostream>
#include "gtest/gtest.h"
#include "callback_manager.h"
#include "mockcpp/mokc.h"

#define private public
#include "rmrs_config.h"
#include "rmrs_migrate_module.h"
#include "rmrs_serializer.h"
#include "rmrs_smap_helper.h"
#include "turbo_conf.h"
#include "turbo_def.h"
#include "turbo_ipc_server.h"
#include "turbo_logger.h"
#include "ucache_driver_interaction.h"
#include "ucache_migration_executor.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace turbo::ipc::server;
using namespace rmrs::migrate;
using namespace rmrs::serialization;
using namespace rmrs::migrate;
using namespace rmrs::smap;
using namespace turbo::config;
using namespace std;
using namespace ucache::migrate_excutor;

namespace ucache {

class TestUcacheMigrationExecutor : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[TestUcacheMigrationExecutor SetUp Begin]" << endl;
        cout << "[TestUcacheMigrationExecutor  SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestUcacheMigrationExecutor  TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestUcacheMigrationExecutor  TearDown End]" << endl;
    }
};

uint32_t MockGetLocalNumaIDs(UcacheMigrationExecutor *This, std::vector<int16_t> &nids)
{
    nids = {0, 1};
    return RMRS_OK;
}

void MockeStartMigrationWorker()
{
    return;
}

TEST_F(TestUcacheMigrationExecutor, ExecuteNewMigrationStrategyTestFail)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    MigrationStrategy newStrategy;
    executor.alreadyStarting_.store(true);
    uint32_t ret = executor.ExecuteNewMigrationStrategy(newStrategy);
    EXPECT_EQ(ret, RMRS_ERROR);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, ExecuteNewMigrationStrategyTest)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    executor.alreadyStarting_.store(false);
    MigrationStrategy newStrategy;

    MOCKER_CPP(&UcacheMigrationExecutor::StartMigrationWorker, void (*)())
        .stubs()
        .will(invoke(MockeStartMigrationWorker));

    uint32_t ret = executor.ExecuteNewMigrationStrategy(newStrategy);
    EXPECT_EQ(ret, RMRS_OK);

    MOCKER_CPP(&UcacheMigrationExecutor::IsRemoteWatermarkExceeded, bool (*)(const MigrationStrategy &strategy))
        .stubs()
        .will(returnValue(false));

    ret = executor.ExecuteNewMigrationStrategy(newStrategy);
    EXPECT_EQ(ret, RMRS_OK);

    MOCKER_CPP(&UcacheMigrationExecutor::RecordPids, uint32_t(*)(const MigrationStrategy &strategy))
        .stubs()
        .will(returnValue(1));
    executor.pidsToMigrate_.push_back(1);

    ret = executor.ExecuteNewMigrationStrategy(newStrategy);
    EXPECT_EQ(ret, RMRS_OK);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, ExcuteMigrateFoliosTestSuccess)
{
    uint16_t desNid = 1;
    int16_t srcNid = 0;
    pid_t pid = 2;
    uint32_t success = 0;

    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    MOCKER_CPP(&UcacheMigrationExecutor::IsNidWatermarkExceeded, bool (*)(const int16_t nid))
        .stubs()
        .will(returnValue(true));
    uint32_t ret = executor.ExcuteMigrateFolios(desNid, srcNid, pid, success);
    EXPECT_EQ(ret, RMRS_OK);
    GlobalMockObject::verify();

    MOCKER_CPP(&UcacheMigrationExecutor::IsNidWatermarkExceeded, bool (*)(const int16_t nid))
        .stubs()
        .will(returnValue(false));
    executor.shouldRun_ = false;
    ret = executor.ExcuteMigrateFolios(desNid, srcNid, pid, success);
    EXPECT_EQ(ret, RMRS_OK);
    GlobalMockObject::verify();

    executor.shouldRun_ = true;
    MOCKER_CPP(&UcacheMigrationExecutor::IsNidWatermarkExceeded, bool (*)(const int16_t nid))
        .stubs()
        .will(returnValue(false));
    MOCKER_CPP(&DriverInteraction::MigrateFoliosInfo,
               uint32_t(*)(const int32_t desNid, const int32_t nid, const pid_t pid))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&DriverInteraction::GetMigrateSuccess, uint32_t(*)(struct MigrateSuccess & queryArg))
        .stubs()
        .will(returnValue(RMRS_OK));
    ret = executor.ExcuteMigrateFolios(desNid, srcNid, pid, success);
    EXPECT_EQ(ret, RMRS_OK);
}

TEST_F(TestUcacheMigrationExecutor, ExcuteMigrateFoliosTestFail)
{
    uint16_t desNid = 1;
    int16_t srcNid = 0;
    pid_t pid = 2;
    uint32_t success = 0;

    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    MOCKER_CPP(&UcacheMigrationExecutor::IsNidWatermarkExceeded, bool (*)(const int16_t nid))
        .stubs()
        .will(returnValue(false));
    MOCKER_CPP(&DriverInteraction::MigrateFoliosInfo,
               uint32_t(*)(const int32_t desNid, const int32_t nid, const pid_t pid))
        .stubs()
        .will(returnValue(1));
    executor.shouldRun_ = true;

    u_int32_t ret = executor.ExcuteMigrateFolios(desNid, srcNid, pid, success);
    EXPECT_EQ(ret, RMRS_ERROR);

    GlobalMockObject::verify();

    MOCKER_CPP(&UcacheMigrationExecutor::IsNidWatermarkExceeded, bool (*)(const int16_t nid))
        .stubs()
        .will(returnValue(false));
    MOCKER_CPP(&DriverInteraction::MigrateFoliosInfo,
               uint32_t(*)(const int32_t desNid, const int32_t nid, const pid_t pid))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&DriverInteraction::GetMigrateSuccess, uint32_t(*)(struct MigrateSuccess & queryArg))
        .stubs()
        .will(returnValue(1));
    executor.shouldRun_ = true;

    ret = executor.ExcuteMigrateFolios(desNid, srcNid, pid, success);
    EXPECT_EQ(ret, RMRS_ERROR);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, MigrateFromSingleSrcNidTestWarn)
{
    uint16_t desNid = 1;
    int16_t srcNid = 0;
    pid_t pid = 2;
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    MOCKER_CPP(&UcacheMigrationExecutor::ExcuteMigrateFolios,
               uint32_t(*)(const uint16_t desNid, const int16_t srcNid, const pid_t pid, uint32_t &success))
        .stubs()
        .will(returnValue(0));

    uint32_t ret = executor.MigrateFromSingleSrcNid(desNid, srcNid, pid);
    EXPECT_EQ(ret, RMRS_WARN);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, MigrateFromSingleSrcNidTestFail)
{
    MOCKER_CPP(&UcacheMigrationExecutor::ExcuteMigrateFolios,
               uint32_t(*)(const uint16_t desNid, const int16_t srcNid, const pid_t pid, uint32_t &success))
        .stubs()
        .will(returnValue(1));
    uint16_t desNid = 1;
    int16_t srcNid = 0;
    pid_t pid = 2;
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    uint32_t ret = executor.MigrateFromSingleSrcNid(desNid, srcNid, pid);
    EXPECT_EQ(ret, RMRS_ERROR);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, MigrateFromMultipleSrcNidsTestOK)
{
    uint16_t desNid = 1;
    pid_t pid = 2;
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    MOCKER_CPP(&UcacheMigrationExecutor::GetLocalNumaIDs,
               uint32_t(*)(UcacheMigrationExecutor * This, std::vector<int16_t> & nids))
        .stubs()
        .will(invoke(MockGetLocalNumaIDs));

    MOCKER_CPP(&UcacheMigrationExecutor::MigrateFromSingleSrcNid,
               uint32_t(*)(const uint16_t desNid, const int16_t srcNid, const pid_t pid))
        .stubs()
        .will(returnValue(RMRS_OK));

    uint32_t ret = executor.MigrateFromMultipleSrcNids(desNid, pid);
    EXPECT_EQ(ret, RMRS_OK);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, MigrateFromMultipleSrcNidsTestMigrateFromSingleSrcNidERROR)
{
    uint16_t desNid = 1;
    pid_t pid = 2;
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    MOCKER_CPP(&UcacheMigrationExecutor::GetLocalNumaIDs,
               uint32_t(*)(UcacheMigrationExecutor * This, std::vector<int16_t> & nids))
        .stubs()
        .will(invoke(MockGetLocalNumaIDs));

    MOCKER_CPP(&UcacheMigrationExecutor::MigrateFromSingleSrcNid,
               uint32_t(*)(const uint16_t desNid, const int16_t srcNid, const pid_t pid))
        .stubs()
        .will(returnValue(RMRS_OK))
        .then(returnValue(RMRS_ERROR));

    uint32_t ret = executor.MigrateFromMultipleSrcNids(desNid, pid);
    EXPECT_EQ(ret, RMRS_ERROR);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, MigrateFromMultipleSrcNidsTestWarn)
{
    uint16_t desNid = 1;
    pid_t pid = 2;
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    MOCKER_CPP(&UcacheMigrationExecutor::GetLocalNumaIDs, uint32_t(*)(std::vector<int16_t> & nids))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&UcacheMigrationExecutor::MigrateFromSingleSrcNid,
               uint32_t(*)(const uint16_t desNid, const int16_t srcNid, const pid_t pid))
        .stubs()
        .will(returnValue(1));

    uint32_t ret = executor.MigrateFromMultipleSrcNids(desNid, pid);
    EXPECT_EQ(ret, RMRS_WARN);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, MigrateFromMultipleSrcNidsTestFail)
{
    uint16_t desNid = 1;
    pid_t pid = 2;
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    uint32_t ret = executor.MigrateFromMultipleSrcNids(desNid, pid);
    EXPECT_EQ(ret, RMRS_ERROR);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, MigrateToRemoteNodeTest)
{
    const size_t pidSize = 3;
    uint16_t desNid = 1;
    MigrationStrategy strategy;
    strategy.srcNid = -1;
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    executor.pidsToMigrate_ = {1, 2, 3};

    executor.MigrateToRemoteNode(desNid, strategy);
    EXPECT_EQ(executor.pidsToMigrate_.size(), pidSize);

    MOCKER_CPP(&UcacheMigrationExecutor::IsNidWatermarkExceeded, bool (*)(const int16_t nid))
        .stubs()
        .will(returnValue(false));

    executor.MigrateToRemoteNode(desNid, strategy);
    EXPECT_EQ(executor.pidsToMigrate_.size(), pidSize);

    MOCKER_CPP(&UcacheMigrationExecutor::MigrateFromSingleSrcNid,
               uint32_t(*)(const uint16_t desNid, const int16_t srcNid, const pid_t pid))
        .stubs()
        .will(returnValue(RMRS_WARN));

    executor.MigrateToRemoteNode(desNid, strategy);
    EXPECT_EQ(executor.pidsToMigrate_.size(), pidSize);

    strategy.srcNid = 0;
    executor.MigrateToRemoteNode(desNid, strategy);
    EXPECT_EQ(executor.pidsToMigrate_.size(), 1);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, PerformMigrationTestpidsToMigrate_MigratetoRemoteNode)
{
    const size_t pidSize = 3;
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    MigrationStrategy strategy;
    strategy.desNids.push_back(1);
    executor.pidsToMigrate_ = {1, 2, 3};

    MOCKER_CPP(&UcacheMigrationExecutor::IsRemoteWatermarkExceeded, bool (*)(const MigrationStrategy &strategy))
        .stubs()
        .will(returnValue(false));

    executor.PerformMigration(strategy);
    EXPECT_EQ(executor.pidsToMigrate_.size(), pidSize);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, PerformMigrationTestpidsToMigrate_Empty)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    MigrationStrategy strategy;
    executor.pidsToMigrate_ = {};

    executor.PerformMigration(strategy);
    EXPECT_EQ(executor.pidsToMigrate_.size(), 0);
}

TEST_F(TestUcacheMigrationExecutor, GetLocalNumaIDsTestDirOpenFail)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    std::vector<int16_t> nids = {0, 1, 2, 3};

    MOCKER(opendir).defaults().will(returnValue((DIR *)nullptr));

    uint32_t ret = executor.GetLocalNumaIDs(nids);
    EXPECT_EQ(ret, RMRS_ERROR);

    GlobalMockObject::verify();
}
TEST_F(TestUcacheMigrationExecutor, GetMemTotalForNodeTestFileNotExist)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    uint64_t value = executor.GetMemTotalForNode(1000);
    EXPECT_EQ(value, RMRS_OK);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, GetMemFilePagesForNodeTestFileNotExist)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();

    uint64_t value = executor.GetMemFilePagesForNode(1000);
    EXPECT_EQ(value, RMRS_OK);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, IsNidWatermarkExceededTest)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    int16_t nid = 1;
    MOCKER_CPP(&UcacheMigrationExecutor::GetMemTotalForNode, uint64_t(*)(int16_t nid))
        .stubs()
        .will(returnValue(uint64_t(0)));

    bool ret = executor.IsNidWatermarkExceeded(nid);
    EXPECT_EQ(ret, true);

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, GetContainerdIdByPidContainerIDEmptyTEST)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    pid_t pid;
    FILE *pipe = (FILE *)0;

    MOCKER(popen).defaults().will(returnValue(pipe));

    std::string containerId = executor.GetContainerdIdByPid(pid);
    EXPECT_EQ(containerId, "");

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, GetContainerdIdByPidfgetsSuccessTEST)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    pid_t pid;
    FILE *pipe = (FILE *)0x12345678;
    char buffer[] = "123\n";
    char *str = "1"; // The function fgets returns a valid value.

    MOCKER(popen).defaults().will(returnValue(pipe));
    MOCKER(fgets)
        .defaults()
        .with(outBoundP(buffer, sizeof(buffer)), mockcpp::any(), mockcpp::any())
        .will(returnValue(str));
    MOCKER(pclose).defaults().will(returnValue(0));

    std::string containerId = executor.GetContainerdIdByPid(pid);
    EXPECT_EQ(containerId, "123");

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, GetContainerdIdByPidfgetsNULLTESTpcloseFailed)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    pid_t pid;
    FILE *pipe = (FILE *)0x12345678;
    char buffer[] = "123\n";
    char *str = "1";

    MOCKER(popen).defaults().will(returnValue(pipe));
    MOCKER(fgets)
        .defaults()
        .with(outBoundP(buffer, sizeof(buffer)), mockcpp::any(), mockcpp::any())
        .will(returnValue(str));
    MOCKER(pclose).defaults().will(returnValue(-1));

    std::string containerId = executor.GetContainerdIdByPid(pid);
    EXPECT_EQ(containerId, "123");

    GlobalMockObject::verify();
}

TEST_F(TestUcacheMigrationExecutor, IsRemoteWatermarkExceededTestFail)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    MigrationStrategy strategy;
    strategy.desNids.push_back(1);

    MOCKER_CPP(&UcacheMigrationExecutor::IsNidWatermarkExceeded, bool (*)(const int16_t nid))
        .stubs()
        .will(returnValue(false));

    bool ret = executor.IsRemoteWatermarkExceeded(strategy);
    EXPECT_EQ(ret, false);
}

TEST_F(TestUcacheMigrationExecutor, RecordPidsTestFail)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    MigrationStrategy strategy;
    strategy.pids.push_back(1);

    uint32_t ret = executor.RecordPids(strategy);
    EXPECT_EQ(ret, 0);

    std::string container_id = "mocked-id";

    MOCKER_CPP(&UcacheMigrationExecutor::GetContainerdIdByPid, std::string(*)(UcacheMigrationExecutor *, pid_t))
        .stubs()
        .will(returnValue(container_id));

    ret = executor.RecordPids(strategy);
    EXPECT_EQ(ret, 0);
}

void MockMigrationWorkerTask()
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    while (executor.shouldRun_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

TEST_F(TestUcacheMigrationExecutor, StartMigrationWorkerTest)
{
    MOCKER_CPP(&UcacheMigrationExecutor::MigrationWorkerTask, void (*)()).stubs().will(invoke(MockMigrationWorkerTask));
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    executor.shouldRun_ = true;
    executor.StartMigrationWorker();
    ASSERT_EQ(executor.migrationWorkerThread_.joinable(), true);
    executor.StopMigrationWorker();
    ASSERT_EQ(executor.migrationWorkerThread_.joinable(), false);
    executor.shouldRun_ = false;
}

void MockPerformMigration(const MigrationStrategy &strategy)
{
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    executor.shouldRun_ = false;
    return;
}

TEST_F(TestUcacheMigrationExecutor, MigrationWorkerTaskTest)
{
    MOCKER_CPP(&UcacheMigrationExecutor::PerformMigration, void (*)(const MigrationStrategy &strategy))
        .stubs()
        .will(invoke(MockPerformMigration));
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    MigrationStrategy strategy{};
    executor.shouldRun_ = true;
    executor.currentStrategy_ = strategy;
    executor.MigrationWorkerTask();
    EXPECT_EQ(executor.shouldRun_, false);

    executor.shouldRun_ = true;
    executor.currentStrategy_.reset();
    executor.MigrationWorkerTask();
    EXPECT_EQ(executor.currentStrategy_.has_value(), false);
}

TEST_F(TestUcacheMigrationExecutor, StopCurrentMigrationStrategyTest)
{
    MOCKER_CPP(&UcacheMigrationExecutor::MigrationWorkerTask, void (*)()).stubs().will(invoke(MockMigrationWorkerTask));
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    executor.StartMigrationWorker();
    EXPECT_EQ(executor.StopCurrentMigrationStrategy(), 0);
}

TEST_F(TestUcacheMigrationExecutor, PerformMigrationTest)
{
    MOCKER_CPP(&UcacheMigrationExecutor::IsRemoteWatermarkExceeded, bool (*)(const MigrationStrategy &))
        .stubs()
        .will(returnValue(true));
    UcacheMigrationExecutor &executor = UcacheMigrationExecutor::GetInstance();
    MigrationStrategy strategy{};
    executor.currentStrategy_ = strategy;
    std::vector<pid_t> pids = {1, 2};
    executor.pidsToMigrate_ = pids;
    executor.PerformMigration(executor.currentStrategy_.value());
    EXPECT_EQ(executor.shouldRun_, false);
}
} // namespace ucache