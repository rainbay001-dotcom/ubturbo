/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.‘
 */
#include <gmock/gmock.h>
#include <cstring>
#include <iostream>
#define private public

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "rmrs_json_helper.cpp"
#include "rmrs_libvirt_helper.h"
#include "rmrs_migrate_module.h"
#include "rmrs_memfree_module.h"
#include "rmrs_resource_export.h"
#include "rmrs_smap_helper.h"
#include "rmrs_json_helper.h"
#include "rmrs_serialize.h"
#include "rmrs_rollback_module.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;
using namespace rmrs::smap;
using namespace rmrs::exports;
using namespace rmrs::serialization;
using namespace rmrs::serialize;

namespace rmrs::migrate {

// 测试类
class TestRmrsMigrateModule : public ::testing::Test {
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

 
void GetRemoteNumaList(std::vector<NumaHugePageInfo> &numaHugePageInfoSumList, std::vector<uint16_t> &remoteNumaIdList);
 
void GetLocalVmInfo(std::vector<VmNumaInfo> &allVmNumaInfoInfoList, std::map<pid_t, VmNumaInfo> &VmNumaInfoMap,
                    std::vector<VmDomainInfo> &vmDomainInfos);

void AdjustMigrationRatio(uint64_t difference, uint64_t &accumulatedMigrateMem,
                          std::map<pid_t, VmNumaInfo> &vmNumaInfoMap,
                          const std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam,
                          std::vector<std::tuple<pid_t, uint64_t, uint16_t>> &potentialMigration,
                          std::map<pid_t, uint16_t> &vmFreqPidRatioMap);

TEST_F(TestRmrsMigrateModule, HandlerRollbackFailed1)
{
    TurboByteBuffer inBuffer;
    TurboByteBuffer outBuffer;
    auto res = RmrsRollbackModule::HandlerRollback(inBuffer, outBuffer);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, HandlerRollbackSucceed)
{
    TurboByteBuffer inBuffer;
    TurboByteBuffer outBuffer;
    outBuffer.len = 1;
    std::map<std::string, std::set<rmrs::serialization::BorrowIdInfo>> value;
    RmrsOutStream builder;
    builder << value;
    inBuffer.data = builder.GetBufferPointer();
    inBuffer.len = builder.GetSize();
    MOCKER_CPP(&RmrsRollbackModule::MemBorrowRollback,
        uint32_t(*)(std::map<std::string, std::set<rmrs::serialization::BorrowIdInfo>> & borrowIdsPidsMap))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsRollbackModule::HandlerRollback(inBuffer, outBuffer);
    delete[] inBuffer.data;
    ASSERT_EQ(res, 0);
}

bool TestFromJsonRmrs(std::string &jsonString, std::map<std::string,
    std::set<rmrs::serialization::BorrowIdInfo>> &value)
{
    int oriSize = 10;

    rmrs::serialization::BorrowIdInfo info;
    info.pid = 1;
    info.oriSize = oriSize;
    value["bid"].insert(info);
    return true;
}

TEST_F(TestRmrsMigrateModule, HandlerRollbackSucceed1)
{
    int oriSize = 10;

    TurboByteBuffer inBuffer;
    TurboByteBuffer outBuffer;
        std::map<std::string, std::set<rmrs::serialization::BorrowIdInfo>> value;
    RmrsOutStream builder;
    rmrs::serialization::BorrowIdInfo info;
    info.pid = 1;
    info.oriSize = oriSize;
    value["bid"].insert(info);
    builder << value;
    inBuffer.data = builder.GetBufferPointer();
    inBuffer.len = builder.GetSize();
    MOCKER_CPP(&RmrsRollbackModule::MemBorrowRollback,
        uint32_t(*)(std::map<std::string, std::set<rmrs::serialization::BorrowIdInfo>> & borrowIdsPidsMap))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsRollbackModule::HandlerRollback(inBuffer, outBuffer);
    delete[] inBuffer.data;
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMigrateModule, MemBorrowRollbackFail1)
{
    // 创建一个 unordered_map
    std::map<std::string, std::set<rmrs::serialization::BorrowIdInfo>> borrowIdsPidsMap;

    // 创建一个 unordered_set 并填充 pid_t 值
    std::set<rmrs::serialization::BorrowIdInfo> pidsSet = {{1234, 0}, {5678, 0}, {91011, 0}};

    // 将键值对添加到 unordered_map
    borrowIdsPidsMap["example_key"] = pidsSet;
    MOCKER_CPP(
        &RmrsRollbackModule::FillRollbackVmInfo,
        bool (*)(std::map<pid_t, VmDomainInfo> &vmInfoMap, std::vector<pid_t> &pidList,
        std::unordered_map<pid_t, uint16_t> &vmPidRemoteNumaMap, std::vector<uint16_t> &remoteNumaIdList))
        .stubs()
        .will(returnValue(false));
    auto res = RmrsRollbackModule::MemBorrowRollback(borrowIdsPidsMap);
    ASSERT_EQ(res, 1);
}

bool TestFillRollbackVmInfoRmrs(std::map<pid_t, VmDomainInfo> &vmInfoMap,
    std::vector<pid_t> &pidList, std::unordered_map<pid_t, uint16_t> &vmPidRemoteNumaMap,
    std::vector<uint16_t> &remoteNumaIdList)
{
    remoteNumaIdList.push_back(1);
    return true;
}

RmrsResult TestGetVmInfoImmediately1(std::vector<VmDomainInfo> &vmDomainInfos)
{
    int memSize = 10;

    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = 1;
    vmDomainInfo.localNumaId = 1;
    vmDomainInfo.remoteNumaId = 1;
    vmDomainInfo.localUsedMem = memSize;
    vmDomainInfo.remoteUsedMem = memSize;

    std::cout << "hxl343434" << std::endl;
    vmDomainInfos.push_back(vmDomainInfo);
    return 0;
}

RmrsResult TestGetVmInfoImmediately10(std::vector<VmDomainInfo> &vmDomainInfos)
{
    int memSize = 10;
    int numaId = 4;

    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = 1;
    vmDomainInfo.localNumaId = 0;
    vmDomainInfo.remoteNumaId = numaId;
    vmDomainInfo.localUsedMem = memSize;
    vmDomainInfo.remoteUsedMem = memSize;
    vmDomainInfos.push_back(vmDomainInfo);
    return 0;
}

TEST_F(TestRmrsMigrateModule, MemBorrowRollbackFail2)
{
    // 创建一个 unordered_map
    std::map<std::string, std::set<rmrs::serialization::BorrowIdInfo>> borrowIdsPidsMap;

    // 创建一个 unordered_set 并填充 pid_t 值
    std::set<rmrs::serialization::BorrowIdInfo> pidsSet = {{1234, 0}, {5678, 0}, {91011, 0}};

    // 将键值对添加到 unordered_map
    borrowIdsPidsMap["example_key"] = pidsSet;
    MOCKER_CPP(
        &RmrsRollbackModule::FillRollbackVmInfo,
        bool (*)(std::map<pid_t, VmDomainInfo> &vmInfoMap, std::vector<pid_t> &pidList,
        std::unordered_map<pid_t, uint16_t> &vmPidRemoteNumaMap, std::vector<uint16_t> &remoteNumaIdList))
        .stubs()
        .will(invoke(TestFillRollbackVmInfoRmrs));
    MOCKER_CPP(&RmrsRollbackModule::CanMigrate, bool (*)(std::map<pid_t, VmDomainInfo> &vmInfoMap))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&RmrsRollbackModule::DoMigrateRollback, bool (*)(std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&RmrsSmapHelper::SmapEnableRemoteNuma, uint32_t (*)(int remoteNumaId))
        .stubs()
        .will(returnValue(1));
    auto res = RmrsRollbackModule::MemBorrowRollback(borrowIdsPidsMap);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, MemBorrowRollbackSucceed)
{
    // 创建一个 unordered_map
    std::map<std::string, std::set<rmrs::serialization::BorrowIdInfo>> borrowIdsPidsMap;

    // 创建一个 unordered_set 并填充 pid_t 值
    std::set<rmrs::serialization::BorrowIdInfo> pidsSet = {{1234, 0}, {5678, 0}, {91011, 0}};

    // 将键值对添加到 unordered_map
    borrowIdsPidsMap["example_key"] = pidsSet;
    MOCKER_CPP(
        &RmrsRollbackModule::FillRollbackVmInfo,
        bool (*)(std::map<pid_t, VmDomainInfo> &vmInfoMap, std::vector<pid_t> &pidList,
                 std::unordered_map<pid_t, uint16_t> &vmPidRemoteNumaMap, std::vector<uint16_t> &remoteNumaIdList))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&RmrsRollbackModule::CanMigrate, bool (*)(std::map<pid_t, VmDomainInfo> &vmInfoMap))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&RmrsRollbackModule::DoMigrateRollback, bool (*)(std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap))
        .stubs()
        .will(returnValue(true));
    auto res = RmrsRollbackModule::MemBorrowRollback(borrowIdsPidsMap);
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMigrateModule, FillRollbackVmInfoFailed1)
{
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::vector<pid_t> pidList;
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    std::vector<uint16_t> remoteNumaIdList;
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsRollbackModule::FillRollbackVmInfo(vmInfoMap, pidList, vmPidRemoteNumaMap, remoteNumaIdList);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, FillRollbackVmInfoFailed2)
{
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::vector<pid_t> pidList;
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    std::vector<uint16_t> remoteNumaIdList;
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(1));
    auto res = RmrsRollbackModule::FillRollbackVmInfo(vmInfoMap, pidList, vmPidRemoteNumaMap, remoteNumaIdList);
    ASSERT_EQ(res, false);
}

RmrsResult TestGetVmInfoImmediately(std::vector<VmDomainInfo> &vmDomainInfos)
{
    pid_t pid = 12;
    int localUsedMem = 100;
    int remoteNumaId = 4;

    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = pid;
    vmDomainInfo.localNumaId = 1;
    vmDomainInfo.localUsedMem = localUsedMem;
    vmDomainInfo.remoteNumaId = remoteNumaId;
    vmDomainInfos.push_back(vmDomainInfo);
    return 0;
}

TEST_F(TestRmrsMigrateModule, FillRollbackVmInfoSucceed)
{
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::vector<pid_t> pidList;
    pidList = {1, 12};
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    std::vector<uint16_t> remoteNumaIdList;
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately));
    auto res = RmrsRollbackModule::FillRollbackVmInfo(vmInfoMap, pidList, vmPidRemoteNumaMap, remoteNumaIdList);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, FillRollbackVmInfoSucceed1)
{
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::vector<pid_t> pidList;
    pidList = {1001, 1002, 1003};
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    std::vector<uint16_t> remoteNumaIdList;
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately));
    auto res = RmrsRollbackModule::FillRollbackVmInfo(vmInfoMap, pidList, vmPidRemoteNumaMap, remoteNumaIdList);
    ASSERT_EQ(res, true);
}

RmrsResult TestGetNumaInfoImmediately(std::vector<NumaInfo> &numaInfos)
{
    int totalMem = 1000;
    NumaInfo numaInfo;
    numaInfo.numaMetaInfo.nodeId = "1";
    numaInfo.numaMetaInfo.logicId = 1;
    numaInfo.numaVmInfo.vmTotalMaxMem = totalMem;
    numaInfos.push_back(numaInfo);
    return 0;
}

TEST_F(TestRmrsMigrateModule, CanMigrateFailed1)
{
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = 1;
    vmDomainInfo.localNumaId = 1;
    vmDomainInfo.localUsedMem = 1;
    vmInfoMap[1] = vmDomainInfo;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    pidInfoList.insert({12, 0});
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately));
    auto res = RmrsRollbackModule::CanMigrate(vmInfoMap, pidInfoList);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, CanMigrateFailed2)
{
    int memSize = 100;
    pid_t pid = 12;
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = pid;
    vmDomainInfo.localNumaId = 1;
    vmDomainInfo.localUsedMem = memSize;
    vmInfoMap[1] = vmDomainInfo;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    pidInfoList.insert({12, 0});
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(1));
    auto res = RmrsRollbackModule::CanMigrate(vmInfoMap, pidInfoList);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, CanMigrateFailed3)
{
    int memSize = 100;
    pid_t pid = 12;
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = pid;
    vmDomainInfo.localNumaId = 1;
    vmDomainInfo.localUsedMem = memSize;
    vmInfoMap[1] = vmDomainInfo;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    pidInfoList.insert({12, 0});
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsRollbackModule::CanMigrate(vmInfoMap, pidInfoList);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, CanMigrateSuccess1)
{
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    auto res = RmrsRollbackModule::CanMigrate(vmInfoMap, pidInfoList);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, CanMigrateBackFailed1)
{
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::map<uint16_t, NumaInfo> numaInfoMap;
    pid_t fakePid = 1001; // 假设的进程 ID
    VmDomainInfo vmInfo;

    vmInfo.metaData.pid = fakePid;
    vmInfoMap[fakePid] = vmInfo;

    NumaInfo numaInfo;
    numaInfo.numaMetaInfo.nodeId = "1";
    uint16_t fakeNumaId = 0; // 假设的 NUMA 节点 ID
    numaInfoMap[fakeNumaId] = numaInfo;

    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    pidInfoList.insert({1001, 0});
    auto res = RmrsRollbackModule::CanMigrateBack(vmInfoMap, numaInfoMap, pidInfoList);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, DoMigrateRollbackSucceed)
{
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    pid_t fakePid1 = 1001;
    int numaId1 = 1;
    vmPidRemoteNumaMap[fakePid1] = numaId1;
    pid_t fakePid2 = 1002;
    int numaId2 = 2;
    vmPidRemoteNumaMap[fakePid2] = numaId2;
    pid_t fakePid3 = 1003;
    int numaId3 = 3;
    vmPidRemoteNumaMap[fakePid3] = numaId3;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    pidInfoList.insert({1001, 0});
    pidInfoList.insert({1002, 0});
    pidInfoList.insert({1003, 0});
    MOCKER_CPP(&RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> & remoteNumaIdList, std::vector<pid_t> & pidsIn,
                             std::vector<uint64_t> memSizeList, uint64_t waitTime))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma, RmrsResult(*)(std::vector<pid_t> & vmPids))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsRollbackModule::DoMigrateRollback(vmPidRemoteNumaMap, pidInfoList);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, DoMigrateRollbackFailed1)
{
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    pid_t fakePid1 = 1001;
    int numaId1 = 1;
    vmPidRemoteNumaMap[fakePid1] = numaId1;
    pid_t fakePid2 = 1002;
    int numaId2 = 2;
    vmPidRemoteNumaMap[fakePid2] = numaId2;
    pid_t fakePid3 = 1003;
    int numaId3 = 3;
    vmPidRemoteNumaMap[fakePid3] = numaId3;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    pidInfoList.insert({1001, 0});
    pidInfoList.insert({1002, 0});
    pidInfoList.insert({1003, 0});
    MOCKER_CPP(&RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> & remoteNumaIdList, std::vector<pid_t> & pidsIn,
                             std::vector<uint64_t> memSizeList, uint64_t waitTime))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma, RmrsResult(*)(std::vector<pid_t> & vmPids))
        .stubs()
        .will(returnValue(1));
    auto res = RmrsRollbackModule::DoMigrateRollback(vmPidRemoteNumaMap, pidInfoList);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, DoMigrateRollbackFailed2)
{
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    pidInfoList.insert({1001, 0});
    pidInfoList.insert({1002, 0});
    pidInfoList.insert({1003, 0});
    auto res = RmrsRollbackModule::DoMigrateRollback(vmPidRemoteNumaMap, pidInfoList);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, DoMigrateRollbackFailed3)
{
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    pid_t fakePid1 = 1001;
    int numaId1 = 1;
    vmPidRemoteNumaMap[fakePid1] = numaId1;
    pid_t fakePid2 = 1002;
    int numaId2 = 2;
    vmPidRemoteNumaMap[fakePid2] = numaId2;
    pid_t fakePid3 = 1003;
    int numaId3 = 3;
    vmPidRemoteNumaMap[fakePid3] = numaId3;
    std::set<rmrs::serialization::BorrowIdInfo> pidInfoList;
    pidInfoList.insert({1001, 0});
    pidInfoList.insert({1002, 0});
    pidInfoList.insert({1003, 0});
    MOCKER_CPP(&RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> & remoteNumaIdList, std::vector<pid_t> & pidsIn,
                             std::vector<uint64_t> memSizeList, uint64_t waitTime))
        .stubs()
        .will(returnValue(1));
    auto res = RmrsRollbackModule::DoMigrateRollback(vmPidRemoteNumaMap, pidInfoList);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, MemFreeImplFailed1)
{
    std::vector<uint16_t> numaIds = {1, 2, 3};
    MOCKER_CPP(&ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(1));
    auto res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, MemFreeImplFailed2)
{
    std::vector<uint16_t> numaIds = {1, 2, 3};
    MOCKER_CPP(&ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    ASSERT_EQ(res, 0);
}

void TestSortRemoteNumaByMemUse(std::vector<ReturnNumaInfo> &returnNumaInfoSumList,
                                std::vector<uint16_t> &remoteNumaIdList)
{
    remoteNumaIdList.push_back(1);
}

TEST_F(TestRmrsMigrateModule, MemFreeImplFailed3)
{
    std::vector<uint16_t> numaIds = {1, 2, 3};
    MOCKER_CPP(&ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&RmrsMemFreeModule::SortRemoteNumaByMemUse,
               void (*)(std::vector<rmrs::migrate::ReturnNumaInfo> &returnNumaInfoSumList,
                        std::vector<uint16_t> &remoteNumaIdList))
        .stubs()
        .will(invoke(TestSortRemoteNumaByMemUse));
    auto res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    ASSERT_EQ(res, 0);
}

RmrsResult TestGetNumaInfoImmediately10(std::vector<NumaInfo> &numaInfos)
{
    NumaInfo numaInfo;
    numaInfo.numaMetaInfo.isLocal = true;
    numaInfos.push_back(numaInfo);
    return 0;
}

TEST_F(TestRmrsMigrateModule, MemFreeImplFailed4)
{
    std::vector<uint16_t> numaIds;
    MOCKER_CPP(&ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately10));
    auto res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(numaIds.size(), 0);
}

RmrsResult TestGetNumaInfoImmediately11(std::vector<NumaInfo> &numaInfos)
{
    NumaInfo numaInfo;
    numaInfo.numaMetaInfo.isLocal = false;
    numaInfos.push_back(numaInfo);
    return 0;
}

RmrsResult TestGetNumaInfoImmediately110(std::vector<NumaInfo> &numaInfos)
{
    int hugePage0 = 1000;
    int memTotal = 1000;
    int numaId0 = 5;
    int hugePage1 = 10;
    int numaId1 = 4;

    NumaInfo numaInfo0;
    numaInfo0.numaMetaInfo.isLocal = false;
    numaInfo0.numaMetaInfo.numaId = numaId0;
    numaInfo0.numaMetaInfo.hugePageFree = hugePage1;
    numaInfo0.numaMetaInfo.hugePageTotal = hugePage1;
    numaInfo0.numaMetaInfo.memTotal = hugePage0;
    NumaInfo numaInfo1;
    numaInfo1.numaMetaInfo.isLocal = false;
    numaInfo1.numaMetaInfo.numaId = numaId1;
    numaInfo1.numaMetaInfo.hugePageFree = hugePage1;
    NumaInfo numaInfo2;
    numaInfo2.numaMetaInfo.isLocal = true;
    numaInfo2.numaMetaInfo.hugePageFree = hugePage0;
    numaInfo2.numaMetaInfo.numaId = 0;
    numaInfos.push_back(numaInfo1);
    numaInfos.push_back(numaInfo0);
    numaInfos.push_back(numaInfo2);
    return 0;
}

TEST_F(TestRmrsMigrateModule, MemFreeImplFailed5)
{
    std::vector<uint16_t> numaIds;
    MOCKER_CPP(&ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately11));
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(1));
    auto res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, MemFreeImplFailed6)
{
    std::vector<uint16_t> numaIds;
    MOCKER_CPP(&ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately11));
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(numaIds.size(), 0);
}

TEST_F(TestRmrsMigrateModule, MemFreeImplSuccess1)
{
    std::vector<uint16_t> numaIds;
    MOCKER_CPP(&ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately11));
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately1));
    
    auto res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMigrateModule, MemFreeImplSuccess2)
{
    std::vector<uint16_t> numaIds;
    MOCKER_CPP(&ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately110));
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately10));
    
    MOCKER_CPP(&RmrsMemFreeModule::ReturnRemoteNuma,
        RmrsResult(*)(std::vector<ReturnVmInfo> &canReturnVmInfoList, std::vector<uint16_t> &numaIds))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsMemFreeModule::MemFreeImpl(numaIds);
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMigrateModule, ResolveReturnNumaIdsSucceed)
{
    int memSize = 1024;
    int hugePage = 10;
    std::vector<NumaInfo> numaInfos;
    std::vector<uint16_t> numaIds;
    NumaInfo numaInfo;
    numaInfo.numaMetaInfo.isLocal = false;
    numaInfo.numaMetaInfo.isRemoteAvailable = true;
    numaInfo.numaMetaInfo.hugePageFree = hugePage;
    numaInfo.numaMetaInfo.hugePageTotal = hugePage;
    numaInfo.numaMetaInfo.memTotal = memSize;
    numaInfos.push_back(numaInfo);
    RmrsMemFreeModule::ResolveReturnNumaIds(numaInfos, numaIds);
}

TEST_F(TestRmrsMigrateModule, DistributeNumaInfoSuccced1)
{
    std::vector<NumaInfo> numaInfos;
    std::vector<ReturnNumaInfo> returnNumaInfoSumList;
    std::map<uint16_t, int> localNumaMemFreeMap;
    NumaInfo numaInfo1;
    numaInfo1.numaMetaInfo.isLocal = true;
    NumaInfo numaInfo2;
    numaInfo1.numaMetaInfo.isLocal = false;
    numaInfos.push_back(numaInfo1);
    numaInfos.push_back(numaInfo2);
    RmrsMemFreeModule::DistributeNumaInfo(numaInfos, returnNumaInfoSumList, localNumaMemFreeMap);
}

TEST_F(TestRmrsMigrateModule, GetExceptPidListSuccced)
{
    pid_t pid = 2;
    ProcessPayload payload;
    payload.pid = pid;
    std::vector<ProcessPayload> processPayloadList{payload};
    std::vector<pid_t> pidList{1};
    std::vector<pid_t> exceptPidList;
    ASSERT_EQ(RmrsMemFreeModule::GetExceptPidList(processPayloadList, pidList, exceptPidList), true);
}

TEST_F(TestRmrsMigrateModule, SortRemoteNumaByMemUseSuccced)
{
    std::vector<ReturnNumaInfo> returnNumaInfoSumList;
    std::vector<uint16_t> remoteNumaIdList;
    ReturnNumaInfo returnNumaInfo;
    returnNumaInfo.numaId = 1;
    returnNumaInfo.isLocal = false;
    RmrsMemFreeModule::SortRemoteNumaByMemUse(returnNumaInfoSumList, remoteNumaIdList);
}

TEST_F(TestRmrsMigrateModule, ReturnNumaInfoComparatorSuccced1)
{
    int memSize1 = 2048;
    int memSize2 = 1024;

    ReturnNumaInfo info1;
    ReturnNumaInfo info2;
    info1.memUse = memSize2;
    info2.memUse = memSize1;
    auto res = RmrsMemFreeModule::ReturnNumaInfoComparator(info1, info2);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, ReturnNumaInfoComparatorSuccced2)
{
    int memSize1 = 2048;
    int memSize2 = 1024;
    ReturnNumaInfo info1;
    ReturnNumaInfo info2;
    info1.memUse = memSize1;
    info2.memUse = memSize2;
    auto res = RmrsMemFreeModule::ReturnNumaInfoComparator(info1, info2);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, GetCanReturnVmInfoFailed1)
{
    int index1 = 1;
    int index2 = 2;
    int numaId = 2;
    std::map<uint16_t, int> localNumaMemFreeMap;
    localNumaMemFreeMap[index1] = 1;
    localNumaMemFreeMap[index2] = numaId;
    std::vector<uint16_t> remoteNumaIdList = {1};
    std::vector<VmDomainInfo> vmDomainInfos;
    VmDomainInfo vmDomainInfo;
    vmDomainInfo.remoteNumaId = 1;
    vmDomainInfo.metaData.pageSize = 1;
    vmDomainInfos.push_back(vmDomainInfo);
    RmrsMemFreeModule::GetCanReturnVmInfo(localNumaMemFreeMap, remoteNumaIdList, vmDomainInfos);
}

TEST_F(TestRmrsMigrateModule, ReturnRemoteNumaFailed1)
{
    std::vector<ReturnVmInfo> canReturnVmInfoList;
    std::vector<uint16_t> numaIds;
    ReturnVmInfo returnVmInfo;
    returnVmInfo.remoteNumaId = 1;
    returnVmInfo.pid = 1;
    canReturnVmInfoList.push_back(returnVmInfo);
    MOCKER_CPP(&RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
        RmrsResult(*)(std::vector<uint16_t> & remoteNumaIdsIn, std::vector<pid_t> & pidsIn,
        std::vector<uint64_t> memSizeList,
        uint64_t waitTime))
        .stubs()
    .will(returnValue(1));
    auto res = RmrsMemFreeModule::ReturnRemoteNuma(canReturnVmInfoList, numaIds);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, ReturnRemoteNumaFailed2)
{
    std::vector<ReturnVmInfo> canReturnVmInfoList;
    std::vector<uint16_t> numaIds;
    ReturnVmInfo returnVmInfo;
    returnVmInfo.remoteNumaId = 1;
    returnVmInfo.pid = 1;
    canReturnVmInfoList.push_back(returnVmInfo);
    
    MOCKER_CPP(&RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
        RmrsResult(*)(std::vector<uint16_t> & remoteNumaIdsIn, std::vector<pid_t> & pidsIn,
        std::vector<uint64_t> memSizeList,
        uint64_t waitTime))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma, RmrsResult(*)(std::vector<pid_t> & vmPids))
        .stubs()
        .will(returnValue(1));
    auto res = RmrsMemFreeModule::ReturnRemoteNuma(canReturnVmInfoList, numaIds);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, ReturnRemoteNumaSucceed)
{
    std::vector<ReturnVmInfo> canReturnVmInfoList;
    std::vector<uint16_t> numaIds;
    ReturnVmInfo returnVmInfo;
    returnVmInfo.remoteNumaId = 1;
    returnVmInfo.pid = 1;
    canReturnVmInfoList.push_back(returnVmInfo);
    MOCKER_CPP(&RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> & remoteNumaIdsIn, std::vector<pid_t> & pidsIn,
               std::vector<uint64_t> memSizeList,
                             uint64_t waitTime))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma, RmrsResult(*)(std::vector<pid_t> & vmPids))
        .stubs()
        .will(returnValue(0));
    auto res = RmrsMemFreeModule::ReturnRemoteNuma(canReturnVmInfoList, numaIds);
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMigrateModule, DistributeNumaMemInfoSucceed)
{
    int memSize = 10;
    std::vector<rmrs::NumaInfo> numaInfos;
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList;
    NumaInfo numaInfo;
    numaInfo.numaMetaInfo.numaId = 1;
    numaInfo.numaMetaInfo.hugePageFree = memSize;
    numaInfo.numaMetaInfo.hugePageTotal = memSize;
    numaInfo.numaMetaInfo.isRemoteAvailable = true;
    numaInfo.numaMetaInfo.isLocal = true;
    numaInfos.push_back(numaInfo);
    RmrsMigrateModule::DistributeNumaMemInfo(numaInfos, numaInfoMap, numaHugePageInfoSumList);
}

TEST_F(TestRmrsMigrateModule, GetRemoteNumaListSucceed)
{
    int memSize = 10;
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList;
    std::vector<uint16_t> remoteNumaIdList;
    NumaHugePageInfo numaHugePageInfo;
    numaHugePageInfo.isRemote = true;
    numaHugePageInfo.isLocal = false;
    numaHugePageInfo.numaId = 1;
    numaHugePageInfo.hugePageFree = memSize;
    numaHugePageInfo.hugePageTotal = memSize;
    numaHugePageInfoSumList.push_back(numaHugePageInfo);
    GetRemoteNumaList(numaHugePageInfoSumList, remoteNumaIdList);
}

TEST_F(TestRmrsMigrateModule, GetLocalVmInfoFailed1)
{
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfo> VmNumaInfoMap;
    std::vector<VmDomainInfo> vmDomainInfos;
    GetLocalVmInfo(allVmNumaInfoInfoList, VmNumaInfoMap, vmDomainInfos);
}

TEST_F(TestRmrsMigrateModule, GetLocalVmInfoSucceed1)
{
    int memSize = 10;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfo> VmNumaInfoMap;
    std::vector<VmDomainInfo> vmDomainInfos;
    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = 1;
    vmDomainInfo.localNumaId = 1;
    vmDomainInfo.remoteNumaId = 1;
    vmDomainInfo.localUsedMem = memSize;
    vmDomainInfo.remoteUsedMem = memSize;
    vmDomainInfos.push_back(vmDomainInfo);
    GetLocalVmInfo(allVmNumaInfoInfoList, VmNumaInfoMap, vmDomainInfos);
}

TEST_F(TestRmrsMigrateModule, ISRemoteMemorySufficient1)
{
    uint64_t size = 10240;
    int numaId = 2;
    int hugePage = 2;
    std::vector<uint16_t> remoteNumaIdList = {1};
    NumaHugePageInfo numaHugePageInfo1;
    numaHugePageInfo1.numaId = 1;
    numaHugePageInfo1.hugePageTotal = 1;
    numaHugePageInfo1.hugePageFree = 1;
    numaHugePageInfo1.isLocal = true;
    numaHugePageInfo1.isRemote = false;

    NumaHugePageInfo numaHugePageInfo2;
    numaHugePageInfo2.numaId = numaId;
    numaHugePageInfo2.hugePageTotal = hugePage;
    numaHugePageInfo2.hugePageFree = hugePage;
    numaHugePageInfo2.isLocal = true;
    numaHugePageInfo2.isRemote = false;

    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;
    numaInfoMap[0] = numaHugePageInfo1;
    numaInfoMap[1] = numaHugePageInfo2;
    uint64_t memMigrateTotalSize = size;

    auto res = RmrsMigrateModule::ISRemoteMemorySufficient(remoteNumaIdList, numaInfoMap, memMigrateTotalSize);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, ISPresetMemorySufficient2)
{
    int remoteUsedMem = 102400;
    pid_t pid = 2;
    uint64_t memMigrateTotalSize;
    std::map<pid_t, VmNumaInfo> vmNumaInfoMap;
    std::vector<rmrs::serialization::VMPresetParam> vmPresetParam;
    rmrs::serialization::VMPresetParam vm;
    vm.pid = 1;
    vmPresetParam.push_back(vm);
    VmNumaInfo vmNumaInfo1;
    vmNumaInfo1.pid = 1;
    vmNumaInfo1.remoteUsedMem = remoteUsedMem;
    vmNumaInfoMap[1] = vmNumaInfo1;
    VmNumaInfo vmNumaInfo2;
    vmNumaInfo2.pid = pid;
    vmNumaInfo2.remoteUsedMem = remoteUsedMem;
    int index2 = 2;
    vmNumaInfoMap[index2] = vmNumaInfo2;
    std::map<pid_t, uint64_t> vmMigratableMemMap;
    auto res = RmrsMigrateModule::ISPresetMemorySufficient(memMigrateTotalSize, vmNumaInfoMap, vmPresetParam,
                                                           vmMigratableMemMap);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, ISPresetMemorySufficient3)
{
    int remoteUsedMem = 1024;
    pid_t pid = 2;
    int index = 2;
    uint64_t memMigrateTotalSize;
    std::map<pid_t, VmNumaInfo> vmNumaInfoMap;
    std::vector<rmrs::serialization::VMPresetParam> vmPresetParam;
    rmrs::serialization::VMPresetParam vm;
    vm.pid = 1;
    vmPresetParam.push_back(vm);
    VmNumaInfo vmNumaInfo1;
    vmNumaInfo1.pid = 1;
    vmNumaInfo1.remoteUsedMem = remoteUsedMem;
    vmNumaInfoMap[1] = vmNumaInfo1;
    VmNumaInfo vmNumaInfo2;
    vmNumaInfo2.pid = pid;
    vmNumaInfo2.remoteUsedMem = remoteUsedMem;
    vmNumaInfoMap[index] = vmNumaInfo2;
    std::map<pid_t, uint64_t> vmMigratableMemMap;
    auto res = RmrsMigrateModule::ISPresetMemorySufficient(memMigrateTotalSize, vmNumaInfoMap, vmPresetParam,
                                                           vmMigratableMemMap);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, GetMigrateOutTime1)
{
    uint64_t memMigrateTotalSize;
    uint32_t waitingTime;
    RmrsMigrateModule::GetMigrateOutTime(memMigrateTotalSize, waitingTime);
}

TEST_F(TestRmrsMigrateModule, MigrateStrategyRmrs1)
{
    uint64_t memMigrateTotalSize;
    std::vector<rmrs::serialization::VMPresetParam> vmPresetParam;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    uint32_t waitingTime;

    int ratio = 30;

    VMPresetParam vmPresetParam1;
    vmPresetParam1.pid = 1;
    vmPresetParam1.ratio = ratio;
    vmPresetParam.push_back(vmPresetParam1);

    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(1));
    auto res =
        RmrsMigrateModule::MigrateStrategyRmrs(memMigrateTotalSize, vmPresetParam, vmMigrateOutParam, waitingTime);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, MigrateStrategyRmrsFail2)
{
    uint64_t memMigrateTotalSize;
    std::vector<rmrs::serialization::VMPresetParam> vmPresetParam;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    uint32_t waitingTime;

    int ratio = 30;
    VMPresetParam vmPresetParam1;
    vmPresetParam1.pid = 1;
    vmPresetParam1.ratio = ratio;
    vmPresetParam.push_back(vmPresetParam1);

    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(0));
    auto res =
        RmrsMigrateModule::MigrateStrategyRmrs(memMigrateTotalSize, vmPresetParam, vmMigrateOutParam, waitingTime);
    ASSERT_EQ(res, 1);
}

RmrsResult TestGetNumaInfoImmediately1(std::vector<NumaInfo> &numaInfos)
{
    int hugePage = 10;
    NumaInfo numaInfo;
    numaInfo.numaMetaInfo.numaId = 1;
    numaInfo.numaMetaInfo.hugePageFree = hugePage;
    numaInfo.numaMetaInfo.hugePageTotal = hugePage;
    numaInfo.numaMetaInfo.isLocal = false;
    numaInfo.numaMetaInfo.isRemoteAvailable = true;

    numaInfos.push_back(numaInfo);
    return 0;
}

TEST_F(TestRmrsMigrateModule, MigrateStrategyRmrsFail3)
{
    uint64_t memMigrateTotalSize;
    std::vector<rmrs::serialization::VMPresetParam> vmPresetParam;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    uint32_t waitingTime;

    int ratio = 30;
    VMPresetParam vmPresetParam1;
    vmPresetParam1.pid = 1;
    vmPresetParam1.ratio = ratio;
    vmPresetParam.push_back(vmPresetParam1);

    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately1));
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(0));
    auto res =
        RmrsMigrateModule::MigrateStrategyRmrs(memMigrateTotalSize, vmPresetParam, vmMigrateOutParam, waitingTime);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, MigrateStrategyRmrsFail4)
{
    uint64_t memMigrateTotalSize;
    std::vector<rmrs::serialization::VMPresetParam> vmPresetParam;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    uint32_t waitingTime;

    int ratio = 30;

    VMPresetParam vmPresetParam1;
    vmPresetParam1.pid = 1;
    vmPresetParam1.ratio = ratio;
    vmPresetParam.push_back(vmPresetParam1);

    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately1));
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately1));
    MOCKER_CPP(&RmrsMigrateModule::ISRemoteMemorySufficient,
               bool (*)(std::vector<uint16_t> &remoteNumaIdList, std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                        const uint64_t &memMigrateTotalSize))
        .stubs()
        .will(returnValue(false));
    auto res =
        RmrsMigrateModule::MigrateStrategyRmrs(memMigrateTotalSize, vmPresetParam, vmMigrateOutParam, waitingTime);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, MigrateStrategyRmrsFail5)
{
    uint64_t memMigrateTotalSize;
    std::vector<rmrs::serialization::VMPresetParam> vmPresetParam;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    uint32_t waitingTime;

    int ratio = 30;
    VMPresetParam vmPresetParam1;
    vmPresetParam1.pid = 1;
    vmPresetParam1.ratio = ratio;
    vmPresetParam.push_back(vmPresetParam1);

    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately1));
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately1));
    MOCKER_CPP(&RmrsMigrateModule::ISRemoteMemorySufficient,
               bool (*)(std::vector<uint16_t> &remoteNumaIdList, std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                        const uint64_t &memMigrateTotalSize))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&RmrsMigrateModule::ISPresetMemorySufficient,
               bool (*)(const uint64_t &memMigrateTotalSize, const std::map<pid_t, VmNumaInfo> &vmNumaInfoMap,
                        const std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam))
        .stubs()
        .will(returnValue(false));
    auto res =
        RmrsMigrateModule::MigrateStrategyRmrs(memMigrateTotalSize, vmPresetParam, vmMigrateOutParam, waitingTime);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, MigrateStrategyRmrsFail7)
{
    uint64_t memMigrateTotalSize;
    std::vector<rmrs::serialization::VMPresetParam> vmPresetParam;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    uint32_t waitingTime;

    int ratio = 30;
    VMPresetParam vmPresetParam1;
    vmPresetParam1.pid = 1;
    vmPresetParam1.ratio = ratio;
    vmPresetParam.push_back(vmPresetParam1);

    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately1));
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately1));
    MOCKER_CPP(&RmrsMigrateModule::ISRemoteMemorySufficient,
               bool (*)(std::vector<uint16_t> &remoteNumaIdList, std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                        const uint64_t &memMigrateTotalSize))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&RmrsMigrateModule::ISPresetMemorySufficient,
               bool (*)(const uint64_t &memMigrateTotalSize, const std::map<pid_t, VmNumaInfo> &vmNumaInfoMap,
                        const std::vector<rmrs::serialization::VMPresetParam> &vmPresetParam))
        .stubs()
        .will(returnValue(false));
    auto res =
        RmrsMigrateModule::MigrateStrategyRmrs(memMigrateTotalSize, vmPresetParam, vmMigrateOutParam, waitingTime);
    ASSERT_EQ(res, 1);
}

// 成功迁出场景
TEST_F(TestRmrsMigrateModule, ISPresetMemorySufficient_1)
{
    // 需要迁出的内存大小, 单位KB
    const uint64_t memMigrateTotalSize = 1048576;
    std::map<pid_t, VmNumaInfo> vmNumaInfoMap;
    std::vector<VMPresetParam> vmPresetParam;

    // 插入示例数据
    int index1 = 111;
    int index2 = 222;
    int index3 = 333;
    vmNumaInfoMap[index1] = {111, 0, 4, 2048 * 1024, 0};
    vmNumaInfoMap[index2] = {222, 0, 5, 4096 * 1024, 0};
    vmNumaInfoMap[index3] = {333, 0, 5, 1024 * 1024, 2048 * 1024};

    // pid和最大迁出比例
    vmPresetParam = {
        {111, 60},
        {222, 60},
        {333, 80}
    };
    std::map<pid_t, uint64_t> vmMigratableMemMap;
    bool ret = RmrsMigrateModule::ISPresetMemorySufficient(memMigrateTotalSize, vmNumaInfoMap, vmPresetParam,
                                                           vmMigratableMemMap);
    ASSERT_EQ(ret, true);
}

TEST_F(TestRmrsMigrateModule, DoMigrateExecuteFailed1)
{
    rmrs::serialization::VMMigrateOutParam param;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    vmMigrateOutParam.push_back(param);
    MOCKER_CPP(RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> &, std::vector<pid_t> &, std::vector<uint64_t>, uint64_t))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    MOCKER_CPP(
        RmrsMigrateModule::RollbackVmMigrate,
        void (*)(const std::vector<rmrs::serialization::VMMigrateOutParam> &, std::unordered_map<pid_t, uint64_t>))
        .stubs()
        .will(ignoreReturnValue());
    MOCKER_CPP(&ResourceExport::GetVmInfoImmediately, RmrsResult(*)(std::vector<VmDomainInfo> &))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    bool ret = RmrsMigrateModule::DoMigrateExecute(vmMigrateOutParam, 0);
    EXPECT_EQ(ret, RMRS_ERROR);
}

TEST_F(TestRmrsMigrateModule, DoMigrateExecuteFailed2)
{
    rmrs::serialization::VMMigrateOutParam param;
    param.memSize = 0;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    vmMigrateOutParam.push_back(param);
    MOCKER_CPP(RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> &, std::vector<pid_t> &, std::vector<uint64_t>, uint64_t))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    MOCKER_CPP(RmrsMigrateModule::RollbackVmMigrate,
        void(*)(const std::vector<rmrs::serialization::VMMigrateOutParam> &, std::unordered_map<pid_t, uint64_t>))
        .stubs()
        .will(ignoreReturnValue());
    MOCKER_CPP(RmrsMigrateModule::FillVmOriginSize, RmrsResult(*)(
        std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParamList,
        std::unordered_map<pid_t, uint64_t> &vmOriginSizeMap))
        .stubs().will(returnValue(RMRS_OK));
    auto ret = RmrsMigrateModule::DoMigrateExecute(vmMigrateOutParam, 0);
    EXPECT_EQ(ret, RMRS_ERROR);
}

TEST_F(TestRmrsMigrateModule, DoMigrateExecuteSucceed)
{
    rmrs::serialization::VMMigrateOutParam param;
    param.memSize = 0;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;
    vmMigrateOutParam.push_back(param);
    MOCKER_CPP(RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> &, std::vector<pid_t> &, std::vector<uint64_t>, uint64_t))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma, RmrsResult(*)(std::vector<pid_t> &))
        .stubs().will(returnValue(RMRS_ERROR));

    MOCKER_CPP(RmrsMigrateModule::FillVmOriginSize, RmrsResult(*)(
        std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParamList,
        std::unordered_map<pid_t, uint64_t> &vmOriginSizeMap))
        .stubs().will(returnValue(RMRS_OK));
    auto ret = RmrsMigrateModule::DoMigrateExecute(vmMigrateOutParam, 0);
    EXPECT_EQ(ret, RMRS_OK);
}

TEST_F(TestRmrsMigrateModule, RollbackVmMigrate1)
{
    rmrs::serialization::VMMigrateMultiOutParam param;
    param.memSize = {0};
    std::vector<rmrs::serialization::VMMigrateMultiOutParam> vmMigrateOutParam;
    vmMigrateOutParam.push_back(param);
    MOCKER_CPP(RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> &, std::vector<pid_t> &, std::vector<uint64_t>, uint64_t))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    MOCKER_CPP(RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma, RmrsResult(*)(std::vector<pid_t> &))
        .stubs().will(returnValue(RMRS_OK));
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap;
    EXPECT_NO_THROW(RmrsMigrateModule::RollbackVmMigrate(vmMigrateOutParam, vmOriginSizeMap));
}

TEST_F(TestRmrsMigrateModule, RollbackVmMigrate2)
{
    rmrs::serialization::VMMigrateMultiOutParam param;
    param.memSize = {0};
    std::vector<rmrs::serialization::VMMigrateMultiOutParam> vmMigrateOutParam;
    vmMigrateOutParam.push_back(param);
    MOCKER_CPP(RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> &, std::vector<pid_t> &, std::vector<uint64_t>, uint64_t))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    MOCKER_CPP(RmrsSmapHelper::SmapRemoveVMPidToRemoteNuma, RmrsResult(*)(std::vector<pid_t> &))
        .stubs().will(returnValue(RMRS_ERROR));
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap;
    EXPECT_NO_THROW(RmrsMigrateModule::RollbackVmMigrate(vmMigrateOutParam, vmOriginSizeMap));
}

TEST_F(TestRmrsMigrateModule, RollbackVmMigrate3)
{
    rmrs::serialization::VMMigrateMultiOutParam param;
    param.memSize = {0};
    std::vector<rmrs::serialization::VMMigrateMultiOutParam> vmMigrateOutParam;
    vmMigrateOutParam.push_back(param);
    MOCKER_CPP(RmrsSmapHelper::MigrateColdDataToRemoteNumaSync,
               RmrsResult(*)(std::vector<uint16_t> &, std::vector<pid_t> &, std::vector<uint64_t>, uint64_t))
        .stubs()
        .will(returnValue(RMRS_OK));
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap;
    EXPECT_NO_THROW(RmrsMigrateModule::RollbackVmMigrate(vmMigrateOutParam, vmOriginSizeMap));
}

RmrsResult TestGetNumaInfoImmediately_2(std::vector<NumaInfo> &numaInfos)
{
    rmrs::NumaInfo info_1;
    rmrs::NumaInfo info_2;
    info_1.numaMetaInfo = NumaMetaData{
        .nodeId = "Node4", .isLocal = false, .isRemoteAvailable = true, .hugePageTotal = 1024, .hugePageFree = 1024};
    info_2.numaMetaInfo = NumaMetaData{
        .nodeId = "Node5", .isLocal = false, .isRemoteAvailable = true, .hugePageTotal = 1024, .hugePageFree = 1024};
    numaInfos.push_back(info_1);
    numaInfos.push_back(info_2);
    return 0;
}

// 获取numa信息
TEST_F(TestRmrsMigrateModule, GetNumaData_1)
{
    std::vector<rmrs::NumaInfo> numaInfos;
    std::vector<uint16_t> remoteNumaIdList;
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList;
    rmrs::NumaInfo info_1;
    rmrs::NumaInfo info_2;
    info_1.numaMetaInfo = NumaMetaData{
        .nodeId = "Node4", .isLocal = false, .isRemoteAvailable = true, .hugePageTotal = 1024, .hugePageFree = 1024};
    info_2.numaMetaInfo = NumaMetaData{
        .nodeId = "Node5", .isLocal = false, .isRemoteAvailable = true, .hugePageTotal = 1024, .hugePageFree = 1024};
    numaInfos.push_back(info_1);
    numaInfos.push_back(info_2);

    MOCKER_CPP(&rmrs::exports::ResourceExport::GetNumaInfoImmediately, RmrsResult(*)(std::vector<NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately_2));

    RmrsResult ret = RmrsMigrateModule::GetNumaData(numaInfos, remoteNumaIdList, numaInfoMap, numaHugePageInfoSumList);
    ASSERT_EQ(ret, 0);
}

// 获取numaInfoMap所有numa节点的索引对应的NUMA信息(numaID 大页总数量 大页空闲数量 是否本地 是否远端)，成功场景-1
/*
* 构造成功场景：
* 成功过滤并获取本节点numa信息
*/
TEST_F(TestRmrsMigrateModule, DistributeNumaMemInfo_success_1)
{
    std::vector<rmrs::NumaInfo> numaInfos;
    rmrs::NumaInfo info_1;
    rmrs::NumaInfo info_2;
    info_1.numaMetaInfo = NumaMetaData{
        .nodeId = "Node4", .isLocal = false, .isRemoteAvailable = true, .hugePageTotal = 1024, .hugePageFree = 1024};
    info_2.numaMetaInfo = NumaMetaData{
        .nodeId = "Node5", .isLocal = false, .isRemoteAvailable = true, .hugePageTotal = 1024, .hugePageFree = 1024};
    numaInfos.push_back(info_1);
    numaInfos.push_back(info_2);
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList;
    RmrsMigrateModule::DistributeNumaMemInfo(numaInfos, numaInfoMap, numaHugePageInfoSumList);
    int result = 2;
    ASSERT_EQ(numaHugePageInfoSumList.size(), result);
}

// 判断远端内存是否足够，成功场景-1
/*
* 构造成功场景：
* 远端numa空闲内存大于需要匀出本地内存的情况
*/
TEST_F(TestRmrsMigrateModule, ISRemoteMemorySufficient_success_1)
{
    std::vector<uint16_t> remoteNumaIdList = {4, 5};
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;
    NumaHugePageInfo info_1 = NumaHugePageInfo{
        .numaId = 4, .hugePageTotal = 1024, .hugePageFree = 1024, .isLocal = false, .isRemote = true};
    NumaHugePageInfo info_2 = NumaHugePageInfo{
        .numaId = 5, .hugePageTotal = 1024, .hugePageFree = 1024, .isLocal = false, .isRemote = true};
    int index1 = 4;
    int index2 = 5;
    numaInfoMap[index1] = info_1;
    numaInfoMap[index2] = info_2;
    uint64_t memMigrateTotalSize = 1024;
    bool ret = RmrsMigrateModule::ISRemoteMemorySufficient(remoteNumaIdList, numaInfoMap, memMigrateTotalSize);
    ASSERT_EQ(ret, true);
}

// 获取迁出执行时间，成功场景-2
/*
* 构造成功场景：
* 预计迁出总量所需时间小于smap迁出迁出轮巡时间10S
* 实际输出迁出时间为10001ms
*/
TEST_F(TestRmrsMigrateModule, GetMigrateOutTime_success_2)
{
    uint64_t memMigrateTotalSize = 1024;
    uint32_t waitingTime;
    RmrsMigrateModule::GetMigrateOutTime(memMigrateTotalSize, waitingTime);
    uint32_t result = 10001;
    ASSERT_EQ(waitingTime, result);
}

// 工具函数：初始化 numaQueryInfo
NumaQueryInfo CreateNumaQueryInfo(const std::vector<uint16_t> &remoteIds,
                                  const std::vector<std::pair<uint16_t, uint64_t>> &idFreePairs)
{
    NumaQueryInfo info;
    info.remoteNumaIdList = remoteIds;
    for (const auto &[numaId, freeMem] : idFreePairs) {
        info.numaInfoMap[numaId] =
            NumaHugePageInfo{.numaId = static_cast<uint16_t>(numaId), .hugePageFree = static_cast<uint64_t>(freeMem)};
    }
    return info;
}

// 工具函数：初始化 vmQueryInfo
VMQueryInfo CreateVMQueryInfo(
    const std::vector<std::tuple<pid_t, uint64_t, uint16_t, uint16_t, uint64_t, uint64_t>> &vmData)
{
    VMQueryInfo info;
    for (const auto &[pid, migratableMem, localNuma, remoteNuma, localUsed, remoteUsed] : vmData) {
        info.vmMigratableMemMap[pid] = migratableMem;
        info.vmNumaInfoMap[pid] = VmNumaInfo{.pid = pid,
                                             .localNumaId = localNuma,
                                             .remoteNumaId = remoteNuma,
                                             .localUsedMem = localUsed,
                                             .remoteUsedMem = remoteUsed};
    }
    return info;
}

// 深度优先搜索算法，失败场景-1
/*
* 构造失败场景：
* 指定两个虚拟机都是已经迁出的，而且绑定的numa都是远端numa4
*/
TEST_F(TestRmrsMigrateModule, DFSGetMigrationActions_failed_1)
{
    // 内存大小单位为 kb
    uint64_t memMigrateTotalSize = 384 * 2048;
    NumaQueryInfo numaQueryInfo;
    VMQueryInfo vmQueryInfo;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;

    // 远端numa为空闲大页个数
    numaQueryInfo.remoteNumaIdList = {4, 5};
    NumaHugePageInfo info_1 = NumaHugePageInfo{.numaId = 4, .hugePageFree = 256};
    NumaHugePageInfo info_2 = NumaHugePageInfo{.numaId = 5, .hugePageFree = 128};
    int index1 = 4;
    int index2 = 5;
    numaQueryInfo.numaInfoMap[index1] = info_1;
    numaQueryInfo.numaInfoMap[index2] = info_2;

    int index3 = 111;
    int index4 = 222;
    pid_t pid1 = 111;
    pid_t pid2 = 222;
    int memSize1 = 266 * 2048;
    int memSize2 = 142 * 2048;
    int memSize3 = 2048 * 2048 - 100 * 2048;
    int memSize4 = 100 * 2048;
    int nodeId = 4;
    vmQueryInfo.vmMigratableMemMap[index3] = memSize1;
    vmQueryInfo.vmMigratableMemMap[index4] = memSize2;

    vmQueryInfo.vmNumaInfoMap[index3] = {.pid = pid1,
                                      .localNumaId = 0,
                                      .remoteNumaId = nodeId,
                                      .localUsedMem = memSize3,
                                      .remoteUsedMem = memSize4};
    vmQueryInfo.vmNumaInfoMap[index4] = {.pid = pid2,
                                      .localNumaId = 0,
                                      .remoteNumaId = nodeId,
                                      .localUsedMem = memSize3,
                                      .remoteUsedMem = memSize4};
    RmrsResult ret =
        RmrsMigrateModule::DFSGetMigrationActions(memMigrateTotalSize, numaQueryInfo, vmQueryInfo, vmMigrateOutParam);
    ASSERT_EQ(ret, 1);
}

TEST_F(TestRmrsMigrateModule, FillVmNumaInfo_failed_1)
{
    NumaQueryInfo numaQueryInfo;
    VMQueryInfo vmQueryInfo;
    std::vector<VmInfo> vms;
    std::vector<NumaInfoWithSize> numas;
    int index1 = 4;
    int index2 = 5;

    int hugePage1 = 256;
    int hugePage2 = 128;

    numaQueryInfo.remoteNumaIdList = {index1, index2};
    NumaHugePageInfo info_1 = NumaHugePageInfo{.numaId = index1, .hugePageFree = hugePage1};
    NumaHugePageInfo info_2 = NumaHugePageInfo{.numaId = index1, .hugePageFree = hugePage2};

    numaQueryInfo.numaInfoMap[index1] = info_1;
    numaQueryInfo.numaInfoMap[index2] = info_2;

    int index3 = 111;
    int index4 = 222;
    pid_t pid1 = 111;
    pid_t pid2 = 222;
    int memSize1 = 266 * 2048;
    int memSize2 = 142 * 2048;
    int memSize3 = 2048 * 2048 - 100 * 2048;
    int memSize4 = 100 * 2048;
    int nodeId = 4;
    vmQueryInfo.vmMigratableMemMap[index3] = memSize1;
    vmQueryInfo.vmMigratableMemMap[index4] = memSize2;

    vmQueryInfo.vmNumaInfoMap[index3] = {.pid = pid1,
                                      .localNumaId = 0,
                                      .remoteNumaId = nodeId,
                                      .localUsedMem = memSize3,
                                      .remoteUsedMem = memSize4};
    vmQueryInfo.vmNumaInfoMap[index4] = {.pid = pid2,
                                      .localNumaId = 0,
                                      .remoteNumaId = nodeId,
                                      .localUsedMem = memSize3,
                                      .remoteUsedMem = memSize4};

    RmrsMigrateModule::FillVmNumaInfo(numaQueryInfo, vmQueryInfo, vms, numas);

    for (const auto &vm : vms) {
        std::cout << "pid: " << vm.pid << ", maxSize: " << vm.maxSize << ", desNumaId: " << vm.desNumaId << std::endl;
    }

    for (const auto &numa : numas) {
        std::cout << "remoteNumaId: " << numa.remoteNumaId << ", freeSize: " << numa.freeSize << std::endl;
    }
    ASSERT_EQ(numaQueryInfo.remoteNumaIdList.size(), numas.size());
    ASSERT_EQ(vmQueryInfo.vmMigratableMemMap.size(), vms.size());
}

TEST_F(TestRmrsMigrateModule, AllocateMemory_failed_1)
{
    uint64_t totalSize = 384 * 2048;
    std::vector<VmInfo> vms;
    std::vector<NumaInfoWithSize> numas;
    std::vector<rmrs::serialization::VMMigrateOutParam> result;

    vms.push_back(VmInfo{.pid = 111, .maxSize = 266 * 2048, .desNumaId = 4});
    vms.push_back(VmInfo{.pid = 222, .maxSize = 142 * 2048, .desNumaId = 4});
    numas.push_back(NumaInfoWithSize{.remoteNumaId = 4, .freeSize = 256 * 2048});
    numas.push_back(NumaInfoWithSize{.remoteNumaId = 5, .freeSize = 128 * 2048});

    RmrsResult ret =
        RmrsMigrateModule::AllocateMemory(totalSize, vms, numas, result);
    ASSERT_EQ(ret, false);
}

// 深度优先搜索算法，失败场景-2
/*
* 构造失败场景：
* 远端numa空闲内存不满足需要迁出本地内存大小
*/
TEST_F(TestRmrsMigrateModule, DFSGetMigrationActions_failed_2)
{
    // 内存大小单位为 kb
    uint64_t memMigrateTotalSize = 384 * 2048;
    NumaQueryInfo numaQueryInfo;
    VMQueryInfo vmQueryInfo;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;

    numaQueryInfo.remoteNumaIdList = {4, 5};
    NumaHugePageInfo info_1 = NumaHugePageInfo{.numaId = 4, .hugePageFree = 256};
    NumaHugePageInfo info_2 = NumaHugePageInfo{.numaId = 5, .hugePageFree = 100};
    int index1 = 4;
    int index2 = 5;
    int index3 = 111;
    int index4 = 222;
    uint64_t memSize1 = 0;
    uint64_t memSize2 = 1;
    numaQueryInfo.numaInfoMap[index1] = info_1;
    numaQueryInfo.numaInfoMap[index2] = info_2;

    vmQueryInfo.vmMigratableMemMap[index3] = memSize1;
    vmQueryInfo.vmMigratableMemMap[index4] = memSize2;

    vmQueryInfo.vmNumaInfoMap[index3] = {
        .pid = 111, .localNumaId = 0, .remoteNumaId = 0, .localUsedMem = 2048 * 2048, .remoteUsedMem = 0};
    vmQueryInfo.vmNumaInfoMap[index4] = {
        .pid = 222, .localNumaId = 0, .remoteNumaId = 0, .localUsedMem = 2048 * 2048, .remoteUsedMem = 0};

    RmrsResult ret =
        RmrsMigrateModule::DFSGetMigrationActions(memMigrateTotalSize, numaQueryInfo, vmQueryInfo, vmMigrateOutParam);
    ASSERT_EQ(ret, 1);
}

TEST_F(TestRmrsMigrateModule, AllocateMemory_failed_2)
{
    uint64_t totalSize = 384 * 2048;
    std::vector<VmInfo> vms;
    std::vector<NumaInfoWithSize> numas;
    std::vector<rmrs::serialization::VMMigrateOutParam> result;

    vms.push_back(VmInfo{.pid = 111, .maxSize = 266 * 2048, .desNumaId = 0});
    vms.push_back(VmInfo{.pid = 222, .maxSize = 142 * 2048, .desNumaId = 0});
    numas.push_back(NumaInfoWithSize{.remoteNumaId = 4, .freeSize = 256 * 2048});
    numas.push_back(NumaInfoWithSize{.remoteNumaId = 5, .freeSize = 100 * 2048});

    RmrsResult ret =
        RmrsMigrateModule::AllocateMemory(totalSize, vms, numas, result);
    ASSERT_EQ(ret, false);
}

// 深度优先搜索算法，失败场景-3
/*
* 构造失败场景：
* 虚拟机可以迁出的内存大小小于需要迁出本地内存大小
*/
TEST_F(TestRmrsMigrateModule, DFSGetMigrationActions_failed_3)
{
    // 内存大小单位为 kb
    uint64_t memMigrateTotalSize = 384 * 2048;
    NumaQueryInfo numaQueryInfo;
    VMQueryInfo vmQueryInfo;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParam;

    numaQueryInfo.remoteNumaIdList = {4, 5};
    NumaHugePageInfo info_1 = NumaHugePageInfo{.numaId = 4, .hugePageFree = 256};
    NumaHugePageInfo info_2 = NumaHugePageInfo{.numaId = 5, .hugePageFree = 128};
    int index1 = 4;
    int index2 = 5;
    int index3 = 111;
    int index4 = 222;
    int memSize1 = 266 * 2048;
    int memSize2 = 100 * 2048;
    int memSize3 = 2048 * 2048;
    pid_t pid1 = 111;
    pid_t pid2 = 222;
    numaQueryInfo.numaInfoMap[index1] = info_1;
    numaQueryInfo.numaInfoMap[index2] = info_2;

    vmQueryInfo.vmMigratableMemMap[index3] = memSize1;
    vmQueryInfo.vmMigratableMemMap[index4] = memSize2;

    vmQueryInfo.vmNumaInfoMap[index3] = {
        .pid = pid1, .localNumaId = 0, .remoteNumaId = 0, .localUsedMem = memSize3, .remoteUsedMem = 0};
    vmQueryInfo.vmNumaInfoMap[index4] = {
        .pid = pid2, .localNumaId = 0, .remoteNumaId = 0, .localUsedMem = memSize3, .remoteUsedMem = 0};

    RmrsResult ret =
        RmrsMigrateModule::DFSGetMigrationActions(memMigrateTotalSize, numaQueryInfo, vmQueryInfo, vmMigrateOutParam);
    ASSERT_EQ(ret, 1);
}

TEST_F(TestRmrsMigrateModule, AllocateMemory_failed_3)
{
    uint64_t totalSize = 384 * 2048;
    std::vector<VmInfo> vms;
    std::vector<NumaInfoWithSize> numas;
    std::vector<rmrs::serialization::VMMigrateOutParam> result;

    int index1 = 111;
    int index2 = 222;
    int memSize1 = 266 * 2048;
    int memSize2 = 100 * 2048;
    int memSize3 = 256 * 2048;
    int memSize4 = 128 * 2048;
    int numaId1 = 4;
    int numaId2 = 5;
    vms.push_back(VmInfo{.pid = index1, .maxSize = memSize1, .desNumaId = 0});
    vms.push_back(VmInfo{.pid = index2, .maxSize = memSize2, .desNumaId = 0});
    numas.push_back(NumaInfoWithSize{.remoteNumaId = numaId1, .freeSize = memSize3});
    numas.push_back(NumaInfoWithSize{.remoteNumaId = numaId2, .freeSize = memSize4});

    RmrsResult ret =
        RmrsMigrateModule::AllocateMemory(totalSize, vms, numas, result);
    ASSERT_EQ(ret, false);
}

int TestGetSmapQueryProcessConfigFuncMock(int nid, struct ProcessPayload *result, int inLen, int *outLen)
{
    *outLen = 1;
    ProcessPayload payload;
    result[0] = payload;
    return 0;
}

int TestGetSmapQueryProcessConfigFuncMockFailed(int nid, struct ProcessPayload *result, int inLen, int *outLen)
{
    return 1;
}

TEST_F(TestRmrsMigrateModule, TestGetSmapQueryProcessConfigFuncFailByDlsym)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    SmapModule::GetSmapQueryProcessConfigFunc();
}

TEST_F(TestRmrsMigrateModule, TestGetSmapQueryProcessConfigFuncSuccess)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(&TestGetSmapQueryProcessConfigFuncMock)));
    SmapModule::GetSmapQueryProcessConfigFunc();
    SmapModule::GetSmapQueryProcessConfigFunc();
}

TEST_F(TestRmrsMigrateModule, TestSmapQueryProcessConfigHelperSuccess)
{
    MOCKER_CPP(&SmapModule::GetSmapQueryProcessConfigFunc, SmapQueryProcessConfigFunc(*)())
        .stubs()
        .will(returnValue(&TestGetSmapQueryProcessConfigFuncMock));
    std::vector<ProcessPayload> processPayloadList;
    RmrsResult res = RmrsSmapHelper::SmapQueryProcessConfigHelper(0, processPayloadList);
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMigrateModule, TestGetMigrateOutMsgByMemSizeFailed)
{
    MigrateOutMsg migrateOutMsg;
    std::vector<pid_t> pidList{1};
    std::vector<uint16_t> remoteNumaIdList{1, 2};
    std::vector<uint64_t> memSizeList{1};
    bool res = RmrsSmapHelper::GetMigrateOutMsgByMemSize(migrateOutMsg, pidList, remoteNumaIdList, memSizeList);
    ASSERT_EQ(res, false);
}

TEST_F(TestRmrsMigrateModule, TestGetMigrateOutMsgByMemSizeSuccess)
{
    MigrateOutMsg migrateOutMsg;
    std::vector<pid_t> pidList{1};
    std::vector<uint16_t> remoteNumaIdList{1};
    std::vector<uint64_t> memSizeList{1};
    bool res = RmrsSmapHelper::GetMigrateOutMsgByMemSize(migrateOutMsg, pidList, remoteNumaIdList, memSizeList);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMigrateModule, TestFillVmOriginSizeFailed_GetVmInfoError)
{
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(1));
    rmrs::serialization::VMMigrateOutParam param;
    pid_t pid = 12;
    param.pid = pid;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParamList{param};
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap;
    RmrsResult res = RmrsMigrateModule::FillVmOriginSize(vmMigrateOutParamList, vmOriginSizeMap);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, TestFillVmOriginSizeFailed_EmptyVmInfo)
{
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(0));
    rmrs::serialization::VMMigrateOutParam param;
    pid_t pid = 12;
    param.pid = pid;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParamList{param};
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap;
    RmrsResult res = RmrsMigrateModule::FillVmOriginSize(vmMigrateOutParamList, vmOriginSizeMap);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, TestFillVmOriginSizeFailed_NotMatchPid)
{
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately));
    rmrs::serialization::VMMigrateOutParam param;
    pid_t pid = 123;
    param.pid = pid;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParamList{param};
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap;
    RmrsResult res = RmrsMigrateModule::FillVmOriginSize(vmMigrateOutParamList, vmOriginSizeMap);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMigrateModule, TestFillVmOriginSizeSuccess)
{
    MOCKER_CPP(&rmrs::exports::ResourceExport::GetVmInfoImmediately,
               RmrsResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately));
    rmrs::serialization::VMMigrateOutParam param;
    pid_t pid = 12;
    param.pid = pid;
    std::vector<rmrs::serialization::VMMigrateOutParam> vmMigrateOutParamList{param};
    std::unordered_map<pid_t, uint64_t> vmOriginSizeMap;
    RmrsResult res = RmrsMigrateModule::FillVmOriginSize(vmMigrateOutParamList, vmOriginSizeMap);
    ASSERT_EQ(res, 0);
}

} // namespace rmrs::migrate