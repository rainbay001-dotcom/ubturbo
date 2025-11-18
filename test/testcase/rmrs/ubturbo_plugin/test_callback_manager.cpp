/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.‘
 */
#include <gmock/gmock.h>
#include <cstring>
#include <iostream>

#define private public
#include "callback_manager.h"
#undef private

#include "gtest/gtest.h"
#include "bottleneck_detector.h"
#include "mockcpp/mokc.h"
#include "rmrs_config.h"
#include "rmrs_json_util.h"
#include "rmrs_libvirt_helper.h"
#include "rmrs_memfree_module.h"
#include "rmrs_migrate_module.h"
#include "rmrs_resource_export.h"
#include "rmrs_rollback_module.h"
#include "rmrs_serialize.h"
#include "rmrs_serializer.h"
#include "rmrs_smap_helper.h"
#include "turbo_conf.h"
#include "turbo_def.h"
#include "turbo_ipc_server.h"
#include "turbo_logger.h"
#include "ucache_migration_executor.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace turbo::ipc::server;
using namespace rmrs::migrate;
using namespace rmrs::serialization;
using namespace rmrs::migrate;
using namespace rmrs::smap;
using namespace turbo::config;
using namespace std;
using namespace rmrs::exports;
using namespace ucache::migrate_excutor;
using namespace rmrs::serialize;

namespace rmrs {

// 测试类
class TestCallBackManager : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[TestCallBackManager SetUp Begin]" << endl;
        cout << "[TestCallBackManager SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestCallBackManager TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestCallBackManager TearDown End]" << endl;
    }
};

TEST_F(TestCallBackManager, InitSucceed)
{
    auto res = CallbackManager::Init();
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, InitFailed1)
{
    MOCKER_CPP(ResourceExport::Init, RmrsResult(*)()).stubs().will(returnValue(1));
    auto res = CallbackManager::Init();
    ASSERT_EQ(res, 1);
}

TEST_F(TestCallBackManager, InitFailed2)
{
    MOCKER_CPP(ResourceExport::Init, RmrsResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(LibvirtHelper::Init, RmrsResult(*)()).stubs().will(returnValue(1));
    auto res = CallbackManager::Init();
    ASSERT_EQ(res, 1);
}

TEST_F(TestCallBackManager, SetResponseSuccess)
{
    RMRSResponseInfoSimpo response;
    RmrsResult retCode;
    std::string msg;
    TurboByteBuffer resBuffer;
    auto res = CallbackManager::SetResponse(response, retCode, msg, resBuffer);
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, SetResponseSuccessWhenBufferFail)
{
    RMRSResponseInfoSimpo response;
    RmrsResult retCode;
    std::string msg;
    TurboByteBuffer resBuffer;
    MOCKER_CPP(&memcpy_s, int (*)(void *dest, size_t destMax, const void *src, size_t count))
        .stubs()
        .will(returnValue(1));
    auto res = CallbackManager::SetResponse(response, retCode, msg, resBuffer);
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, BorrowRollbackRecvHandlerFailed1)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    MOCKER_CPP(&CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(1));
    auto res = CallbackManager::BorrowRollbackRecvHandler(req, resp);
    ASSERT_EQ(res, 1);
}

TEST_F(TestCallBackManager, BorrowRollbackRecvHandlerFailed2)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    MOCKER_CPP(&CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&RmrsRollbackModule::HandlerRollback,
               uint32_t(*)(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer))
        .stubs()
        .will(returnValue(1));
    auto res = CallbackManager::BorrowRollbackRecvHandler(req, resp);
    ASSERT_EQ(res, 1);
}

TEST_F(TestCallBackManager, BorrowRollbackRecvHandlerSucceed)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    MOCKER_CPP(&CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&RmrsRollbackModule::HandlerRollback,
               uint32_t(*)(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer))
        .stubs()
        .will(returnValue(0));
    req.len = 1;
    req.data = new uint8_t[1];
    auto res = CallbackManager::BorrowRollbackRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, 0);
}

uint32_t TestUBTurboGetFloat(const std::string &section, const std::string &configKey, float &configValue)
{
    int value = 10.0;
    configValue = value;
    return 0;
}

TEST_F(TestCallBackManager, InitSmapSucceed1)
{
    CallbackManager::isSetSmapMode = false;
    MOCKER_CPP(&RmrsSmapHelper::SmapMode, RmrsResult(*)(int runMode)).stubs().will(returnValue(0));
    auto res = CallbackManager::InitSmap();
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, InitSmapSucceed2)
{
    CallbackManager::isSetSmapMode = false;
    MOCKER_CPP(&UBTurboGetFloat,
               uint32_t(*)(const std::string &section, const std::string &configKey, float &configValue))
        .stubs()
        .will(invoke(TestUBTurboGetFloat));
    MOCKER_CPP(&RmrsSmapHelper::SmapMode, RmrsResult(*)(int runMode)).stubs().will(returnValue(0));
    auto res = CallbackManager::InitSmap();
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, InitSmapFailed1)
{
    CallbackManager::isSetSmapMode = false;
    MOCKER_CPP(&RmrsSmapHelper::SmapMode, RmrsResult(*)(int runMode)).stubs().will(returnValue(1));
    auto res = CallbackManager::InitSmap();
    ASSERT_EQ(res, 1);
}

TEST_F(TestCallBackManager, InitSmapFailed2)
{
    CallbackManager::isSetSmapMode = false;
    MOCKER_CPP(&UBTurboGetFloat,
               uint32_t(*)(const std::string &section, const std::string &configKey, float &configValue))
        .stubs()
        .will(invoke(TestUBTurboGetFloat));
    MOCKER_CPP(&RmrsSmapHelper::SmapMode, RmrsResult(*)(int runMode)).stubs().will(returnValue(1));
    auto res = CallbackManager::InitSmap();
    ASSERT_EQ(res, 1);
}

TEST_F(TestCallBackManager, MigrateBackRecvHandlerSucceed2)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    MOCKER_CPP(&CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&RmrsMemFreeModule::MemFreeImpl, RmrsResult(*)(std::vector<uint16_t> & numaIds, uint64_t & memSize))
        .stubs()
        .will(returnValue(0));
    req.len = 1;
    req.data = new uint8_t[1];
    auto res = CallbackManager::MigrateBackRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, MigrateBackRecvHandlerFailed3)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    MOCKER_CPP(&CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&RmrsMemFreeModule::MemFreeImpl, RmrsResult(*)(std::vector<uint16_t> & numaIds, uint64_t & memSize))
        .stubs()
        .will(returnValue(1));
    auto res = CallbackManager::MigrateBackRecvHandler(req, resp);
    ASSERT_EQ(res, 1);
}

TEST_F(TestCallBackManager, MigrateBackRecvHandlerFailed5)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    MOCKER_CPP(&CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(1));
    auto res = CallbackManager::MigrateBackRecvHandler(req, resp);
    ASSERT_EQ(res, 1);
}

TEST_F(TestCallBackManager, HeartBeatRecvHandlerSucceed)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    auto res = CallbackManager::HeartBeatRecvHandler(req, resp);
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, MigrateStrategyRecvHandlerFailed2)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    MOCKER_CPP(&CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(1));
    auto res = CallbackManager::MigrateStrategyRecvHandler(req, resp);
    ASSERT_EQ(res, 1);
}

static RmrsResult MigrateStrategyRmrsForTest(const uint64_t &memMigrateTotalSize,
                                             const std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
                                             std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam,
                                             uint32_t &waitingTime)
{
    rmrs::serialization::VMMigrateOutParam param;
    vmMigrateOutParam.push_back(param);
    return 0;
}

TEST_F(TestCallBackManager, MigrateStrategyRecvHandlerSucceed)
{
    int memSize = 1024;
    TurboByteBuffer req;
    TurboByteBuffer resp;
    VMPresetParam vm;
    vm.pid = 1;
    std::vector<VMPresetParam> vmInfoList1;
    vmInfoList1.push_back(vm);
    MigrateStrategyParamRMRS msp;
    msp.borrowSize = memSize;
    msp.vmInfoList = vmInfoList1;
    RmrsOutStream builder;
    builder << msp;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(&CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(
        RmrsMigrateModule::MigrateStrategyRmrs,
        RmrsResult(*)(const uint64_t &memMigrateTotalSize,
                      const std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
                      std::vector<rmrs::serialization::VMMigrateOutParam> &vmMigrateOutParam, uint32_t &waitingTime))
        .stubs()
        .will(invoke(MigrateStrategyRmrsForTest));
    auto res = CallbackManager::MigrateStrategyRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, PidNumaInfoCollectRecvHandlerFailed1)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    PidNumaInfoCollectParam param;
    param.pidList.push_back(0);
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(&ResourceExport::CollectPidNumaInfo,
               RmrsResult(*)(const std::vector<pid_t> &, std::vector<mempooling::PidInfo> &))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    auto ret = CallbackManager::PidNumaInfoCollectRecvHandler(req, resp);
    EXPECT_EQ(ret, RMRS_ERROR);
}

RmrsResult MockCollectPidNumaInfoSuccess(const std::vector<pid_t> &pids, std::vector<mempooling::PidInfo> &pidInfos)
{
    pid_t pid = 1234;
    int localUsedMem = 10;
    int remoteUsedMem = 5;
    int remoteNumaId = 4;
    mempooling::PidInfo info;
    info.pid = pid;
    info.localUsedMem = localUsedMem;
    info.remoteUsedMem = remoteUsedMem;
    info.localNumaIds = {1};
    info.remoteNumaId = remoteNumaId;
    pidInfos.emplace_back(info);
    return RMRS_OK;
}

TEST_F(TestCallBackManager, PidNumaInfoCollectRecvHandlerSucceed)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    PidNumaInfoCollectParam param;
    param.pidList.push_back(1);
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(&ResourceExport::CollectPidNumaInfo,
               RmrsResult(*)(const std::vector<pid_t> &, std::vector<mempooling::PidInfo> &))
        .stubs()
        .will(invoke(MockCollectPidNumaInfoSuccess));
    auto ret = CallbackManager::PidNumaInfoCollectRecvHandler(req, resp);
    EXPECT_EQ(ret, RMRS_OK);
}

TEST_F(TestCallBackManager, MigrateExecuteRecvHandlerSucceed2)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    rmrs::serialization::MigrateStrategyResult param;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(RMRS_OK));
    MOCKER_CPP(&RmrsMigrateModule::DoMigrateExecute,
               RmrsResult(*)(const std::vector<rmrs::serialization::VMMigrateOutParam> &, uint64_t))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    MOCKER_CPP((bool (*)(const std::filesystem::path &))(std::filesystem::exists),
               bool (*)(const std::filesystem::path &))
        .stubs()
        .will(returnValue(false));

    auto res = CallbackManager::MigrateExecuteRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, RMRS_OK);
}

TEST_F(TestCallBackManager, MigrateExecuteRecvHandlerFailed1)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    rmrs::serialization::MigrateStrategyResult param;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(RMRS_ERROR));
    auto res = CallbackManager::MigrateExecuteRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, RMRS_ERROR);
}

TEST_F(TestCallBackManager, MigrateExecuteRecvHandlerSucceed)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    rmrs::serialization::MigrateStrategyResult param;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(CallbackManager::InitSmap, RmrsResult(*)()).stubs().will(returnValue(RMRS_OK));
    MOCKER_CPP(&RmrsMigrateModule::DoMigrateExecute,
               RmrsResult(*)(const std::vector<rmrs::serialization::VMMigrateOutParam> &, uint64_t))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP((bool (*)(const std::filesystem::path &))(std::filesystem::exists),
               bool (*)(const std::filesystem::path &))
        .stubs()
        .will(returnValue(false));

    auto res = CallbackManager::MigrateExecuteRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, RMRS_OK);
}

TEST_F(TestCallBackManager, NumaMemInfoCollectRecvHandlerFailed1)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    NumaMemInfoCollectParam param;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(&ResourceExport::CollectPidNumaInfo,
               RmrsResult(*)(const std::vector<pid_t> &, std::vector<mempooling::PidInfo> &))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(rmrs::JsonUtil::RackMemConvertMap2JsonStr, bool (*)(const JSON_MAP &, JSON_STR &))
        .stubs()
        .will(returnValue(false));
    auto res = CallbackManager::NumaMemInfoCollectRecvHandler(req, resp);
    ASSERT_EQ(res, RMRS_ERROR);
}

TEST_F(TestCallBackManager, NumaMemInfoCollectRecvHandlerFailed2)
{
    int numaId = -2;
    TurboByteBuffer req;
    TurboByteBuffer resp;
    NumaMemInfoCollectParam param;
    param.numaId = numaId;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto res = CallbackManager::NumaMemInfoCollectRecvHandler(req, resp);
    ASSERT_EQ(res, RMRS_ERROR);
}

TEST_F(TestCallBackManager, NumaMemInfoCollectRecvHandlerSucceed)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    NumaMemInfoCollectParam param;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(&ResourceExport::CollectPidNumaInfo,
               RmrsResult(*)(const std::vector<pid_t> &, std::vector<mempooling::PidInfo> &))
        .stubs()
        .will(returnValue(RMRS_OK));
    auto res = CallbackManager::NumaMemInfoCollectRecvHandler(req, resp);
    ASSERT_EQ(res, RMRS_OK);
}

TEST_F(TestCallBackManager, UCacheMigrateStrategyRecvHandlerSucceed)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;

    UCacheMigrationStrategyParam param;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MOCKER_CPP(&ucache::migrate_excutor::UcacheMigrationExecutor::ExecuteNewMigrationStrategy,
               uint32_t(*)(const ucache::migrate_excutor::MigrationStrategy &))
        .stubs()
        .will(returnValue(RMRS_OK));
    auto res = CallbackManager::UCacheMigrateStrategyRecvHandler(req, resp);
    ASSERT_EQ(res, RMRS_OK);
}

TEST_F(TestCallBackManager, UCacheMigrateStrategyRecvHandlerSucceed2)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;

    UCacheMigrationStrategyParam param;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MOCKER_CPP(&ucache::migrate_excutor::UcacheMigrationExecutor::ExecuteNewMigrationStrategy,
               uint32_t(*)(const ucache::migrate_excutor::MigrationStrategy &))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    auto res = CallbackManager::UCacheMigrateStrategyRecvHandler(req, resp);
    ASSERT_EQ(res, RMRS_OK);
}

TEST_F(TestCallBackManager, UCacheMigrateStopRecvHandlerSucceed)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;

    UCacheMigrationStrategyParam param;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MOCKER_CPP(&ucache::migrate_excutor::UcacheMigrationExecutor::StopCurrentMigrationStrategy, uint32_t(*)())
        .stubs()
        .will(returnValue(RMRS_OK));
    auto res = CallbackManager::UCacheMigrateStopRecvHandler(req, resp);
    ASSERT_EQ(res, RMRS_OK);
}

TEST_F(TestCallBackManager, UCacheMigrateStrategyRecvHandlerSucceed1)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    UCacheMigrationStrategyParam param;
    std::vector<uint16_t> emptyNumaIds;
    std::vector<pid_t> emptyPids;
    param.localNumaId = 0;
    param.remoteNumaIds = emptyNumaIds;
    param.pids = emptyPids;
    param.ucacheUsageRatio = 0.0;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(&UcacheMigrationExecutor::ExecuteNewMigrationStrategy, uint32_t(*)(const MigrationStrategy &newStrategy))
        .stubs()
        .will(returnValue(0));
    auto res = CallbackManager::UCacheMigrateStrategyRecvHandler(req, resp);
    ASSERT_EQ(res, 0);
}

TEST_F(TestCallBackManager, UpdateUCacheRatioRecvHandlerTestSucceed)
{
    TurboByteBuffer req;
    TurboByteBuffer resp;
    MigrationInfoParam param;
    std::vector<pid_t> emptyPids;
    param.borrowMemKB = 0;
    param.pids = emptyPids;
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    MOCKER_CPP(&ucache::bottleneck_detector::BottleneckDetector::GetUCacheUsagePercentage,
               uint32_t(*)(const rmrs::MigrationInfoParam &info, uint32_t &ucacheUsagePercentage))
        .stubs()
        .will(returnValue(0));
    auto res = CallbackManager::UpdateUCacheRatioRecvHandler(req, resp);
    ASSERT_EQ(res, 0);
}
} // namespace rmrs
