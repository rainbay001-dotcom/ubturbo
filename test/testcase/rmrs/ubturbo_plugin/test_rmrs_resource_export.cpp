/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include <securec.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "os_helper/rmrs_os_helper.h"
#include "rmrs_config.h"
#include "rmrs_error.h"
#include "rmrs_libvirt_helper.h"
#include "rmrs_resource_export.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;

namespace rmrs::exports {

// 测试类
class TestRmrsResourceExport : public ::testing::Test {
protected:
    TestRmrsResourceExport() {}
    // virtual ~TestRmrsResourceExport() {}
    virtual void SetUp() {}
    virtual void TearDown()
    {
        GlobalMockObject::reset();
    }
};

void InitMocker()
{
    MOCKER_CPP(&rmrs::LibvirtHelper::Init, uint32_t(*)()).stubs().will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::GetHostName, uint32_t(*)(string & hostName)).stubs().will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::OsHelper::GetSocketCpuRelation, uint32_t(*)(unordered_map<uint16_t, uint16_t> & cpuSocketMap))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::OsHelper::GetNumaCPUInfos,
               uint32_t(*)(unordered_map<uint16_t, uint16_t> & cpuSocketMap,
                           unordered_map<uint16_t, uint16_t> & cpuNumaMap, vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::OsHelper::GetRemoteAvailableFlag, RmrsResult(*)(vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(RMRS_OK));
}

TEST_F(TestRmrsResourceExport, InitSucceed)
{
    InitMocker();
    ResourceExport resourceExport1;
    RmrsResult res = resourceExport1.Init();
    EXPECT_EQ(res, RMRS_OK);
    GlobalMockObject::verify();
    MOCKER_CPP(&rmrs::LibvirtHelper::GetHostName, uint32_t(*)(string & hostName))
        .stubs()
        .will(returnValue(RMRS_ERROR))
        .then(returnValue(RMRS_OK));
    ResourceExport resourceExport2;
    res = resourceExport2.Init();
    EXPECT_EQ(res, RMRS_OK);
    MOCKER_CPP(&rmrs::OsHelper::GetSocketCpuRelation, uint32_t(*)(unordered_map<uint16_t, uint16_t> & cpuSocketMap))
        .stubs()
        .will(returnValue(RMRS_ERROR))
        .then(returnValue(RMRS_OK));
    ResourceExport resourceExport3;
    res = resourceExport3.Init();
    EXPECT_EQ(res, RMRS_ERROR);
    MOCKER_CPP(&rmrs::OsHelper::GetNumaCPUInfos,
               uint32_t(*)(unordered_map<uint16_t, uint16_t> & cpuSocketMap,
                           unordered_map<uint16_t, uint16_t> & cpuNumaMap, vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(RMRS_ERROR))
        .then(returnValue(RMRS_OK));
    ResourceExport resourceExport4;
    res = resourceExport4.Init();
    EXPECT_EQ(res, RMRS_ERROR);
    ResourceExport resourceExport5;
    res = resourceExport5.Init();
    EXPECT_EQ(res, RMRS_OK);
    MOCKER_CPP(&rmrs::OsHelper::GetRemoteAvailableFlag, RmrsResult(*)(vector<NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    ResourceExport resourceExport6;
    res = resourceExport6.Init();
    EXPECT_EQ(res, RMRS_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestRmrsResourceExport, CollectVmInfoSucceed)
{
    MOCKER_CPP(&rmrs::LibvirtHelper::GetVmBasicInfo, RmrsResult(*)(ResourceExport * vmInfoHandler))
        .stubs()
        .will(returnValue(RMRS_OK))
        .then(returnValue(RMRS_ERROR));
    MOCKER_CPP(&rmrs::LibvirtHelper::Connect, uint32_t(*)()).stubs().will(returnValue(RMRS_OK));
    MOCKER_CPP(&rmrs::LibvirtHelper::CloseConn, uint32_t(*)()).stubs().will(returnValue(RMRS_OK));
    ResourceExport resourceExport;
    RmrsResult res = resourceExport.Init();
    res = resourceExport.CollectVmInfo();
    EXPECT_EQ(res, RMRS_OK);

    res = resourceExport.CollectVmInfo();
    EXPECT_EQ(res, RMRS_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestRmrsResourceExport, UpdateVmDomainInfoSucceed)
{
    MOCKER_CPP(&rmrs::LibvirtHelper::Init, uint32_t(*)()).stubs().will(returnValue(RMRS_OK));
    ResourceExport resourceExport;
    RmrsResult res = resourceExport.Init();
    res = resourceExport.UpdateVmDomainInfo();
    EXPECT_EQ(res, RMRS_OK);

    std::vector<VmDomainInfo> *VmDomainInfos = resourceExport.GetVmDomainInfos();
    std::unordered_map<uint16_t, uint16_t> *vmNumaCpuInfos = resourceExport.GetVmNumaCpuInfos();
    std::unordered_map<uint16_t, std::vector<uint16_t>> *vmNumaCpuIds = resourceExport.GetVmNumaCpuIds();
    std::unordered_map<std::string, uint16_t> *vmNumaCpuNum = resourceExport.GetUuidNumaMap();
    std::unordered_map<uint16_t, uint16_t> *cpuNumaMap = resourceExport.GetCpuNumaMap();
    std::unordered_map<uint16_t, uint16_t> *cpuSocketMap = resourceExport.GetCpuSocketMap();
    std::unordered_map<uint16_t, uint64_t> *vmNumaMemInfos = resourceExport.GetVmNumaMaxMemInfos();
    std::vector<NumaInfo> *numaInfos = resourceExport.GetNumaInfos();
    EXPECT_EQ(res, RMRS_OK);
}

TEST_F(TestRmrsResourceExport, UpdateVmDomainInfoSucceed1)
{
    ResourceExport resourceExport;
    std::vector<VmDomainInfo> *vmDomainInfos = resourceExport.GetVmDomainInfos();
    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.uuid = "1234";
    vmDomainInfo.metaData.pid = 1;
    vmDomainInfos->push_back(vmDomainInfo);
    RmrsResult res = resourceExport.UpdateVmDomainInfo();
    EXPECT_EQ(res, RMRS_OK);
}

TEST_F(TestRmrsResourceExport, GetVmInfoImmediatelyFailed2)
{
    MOCKER_CPP(&ResourceExport::CollectVmInfo, uint32_t(*)()).stubs().will(returnValue(RMRS_ERROR));
    std::vector<VmDomainInfo> vmDomainInfos;
    ResourceExport resourceExport;
    auto res = resourceExport.GetVmInfoImmediately(vmDomainInfos);
    EXPECT_EQ(res, RMRS_ERROR);
}

TEST_F(TestRmrsResourceExport, GetVmInfoImmediatelySucceed1)
{
    MOCKER_CPP(&ResourceExport::CollectVmInfo, uint32_t(*)()).stubs().will(returnValue(RMRS_OK));
    std::vector<VmDomainInfo> vmDomainInfos;
    ResourceExport resourceExport;
    auto res = resourceExport.GetVmInfoImmediately(vmDomainInfos);
    EXPECT_EQ(res, RMRS_OK);
}

TEST_F(TestRmrsResourceExport, GetNumaInfoImmediatelyFailed2)
{
    MOCKER_CPP(&ResourceExport::CollectVmInfo, uint32_t(*)()).stubs().will(returnValue(RMRS_ERROR));
    std::vector<NumaInfo> numaInfos;
    ResourceExport resourceExport;
    auto res = resourceExport.GetNumaInfoImmediately(numaInfos);
    EXPECT_EQ(res, RMRS_ERROR);
}

TEST_F(TestRmrsResourceExport, GetNumaInfoImmediatelySucceed1)
{
    MOCKER_CPP(&ResourceExport::CollectVmInfo, uint32_t(*)()).stubs().will(returnValue(RMRS_OK));
    std::vector<NumaInfo> numaInfos;
    ResourceExport resourceExport;
    auto res = resourceExport.GetNumaInfoImmediately(numaInfos);
    EXPECT_EQ(res, RMRS_OK);
}

std::set<uint16_t> GetLocalNodeIdsForTest()
{
    // 模拟本地节点为 0 和 1
    return {0, 1};
}

int GetNumaIdToSocketIdForTest(uint16_t nodeId)
{
    // 简单映射：nodeId % 2 作为 socketId
    return nodeId % 2;
}

TEST_F(TestRmrsResourceExport, BuildPidInfoFromNodePagesSucceed)
{
    pid_t testPid = 1234;
    std::unordered_map<uint16_t, uint64_t> nodePages = {{0, 20}, {1, 10}, {2, 5}};

    std::set<uint16_t> localNodeIds = {0, 1};

    MOCKER_CPP(ResourceExport::GetNumaIdToSocketId, int (*)(uint16_t)).stubs().will(invoke(GetNumaIdToSocketIdForTest));

    auto pidInfo = ResourceExport::BuildPidInfoFromNodePages(testPid, false, nodePages, localNodeIds);

    int sizeResult = 2;

    // 验证基础字段
    EXPECT_EQ(pidInfo.pid, testPid);
    EXPECT_EQ(pidInfo.remoteNumaId, sizeResult);
    EXPECT_EQ(pidInfo.socketId, 0);

    // 验证本地 NUMA ID 顺序（按使用量降序）
    ASSERT_EQ(pidInfo.localNumaIds.size(), sizeResult);
    EXPECT_EQ(pidInfo.localNumaIds[0], 0); // 20页 > 10页，node 0 排在前面
    EXPECT_EQ(pidInfo.localNumaIds[1], 1);
}

TEST_F(TestRmrsResourceExport, CollectPidNumaInfoFailed)
{
    std::vector<pid_t> pids = {0};
    std::vector<mempooling::PidInfo> pidInfos;

    MOCKER_CPP(ResourceExport::GetLocalNodeIds, std::set<uint16_t>(*)()).stubs().will(invoke(GetLocalNodeIdsForTest));
    auto res = ResourceExport::CollectPidNumaInfo(pids, pidInfos);

    EXPECT_EQ(res, RMRS_ERROR);
}

TEST_F(TestRmrsResourceExport, GetLocalNodeIdsTest)
{
    std::set<uint16_t> res = ResourceExport::GetLocalNodeIds();
    ASSERT_FALSE(res.empty());
}

TEST_F(TestRmrsResourceExport, IsValidLocalNumaNodeTest)
{
    uint64_t localUsedMem = 10;
    uint64_t remoteUsedMem = 5;
    uint64_t remoteNumaId = 4;
    mempooling::PidInfo info;
    info.pid = -1;
    info.localUsedMem = localUsedMem;
    info.remoteUsedMem = remoteUsedMem;
    info.localNumaIds = {1};
    info.remoteNumaId = remoteNumaId;

    std::vector<mempooling::PidInfo> vec = {info};
    std::set<uint16_t> res = ResourceExport::GetLocalNodeIds();
    ASSERT_FALSE(res.empty());
}

TEST_F(TestRmrsResourceExport, CollectMeminfobyNumaIdSuccess)
{
    std::map<std::string, std::string> infoMap;
    auto res = ResourceExport::CollectMeminfobyNumaId(0, infoMap);
    auto it = infoMap.find("MemTotal");
    ASSERT_NE(it, infoMap.end());
    it = infoMap.find("SocketId");
    ASSERT_NE(it, infoMap.end());
    ASSERT_NE(it->second, "-1");
}

TEST_F(TestRmrsResourceExport, ReadFileFirstLineSuccess)
{
    std::string path = "/proc/meminfo";
    std::string line = ReadFileFirstLine(path);
    ASSERT_NE(line, "");
}

TEST_F(TestRmrsResourceExport, ReadFileFirstLineReturnEmpty)
{
    std::string file = "/tmp/empty.txt";
    {
        std::ofstream ofs(file); // 创建空文件
    }
    MOCKER_CPP(IsPathValid, bool (*)(const std::filesystem::path &)).stubs().will(returnValue(true));

    EXPECT_EQ(ReadFileFirstLine(file), "");
}

TEST_F(TestRmrsResourceExport, ReadFileFirstLineReturnPathInvalid)
{
    std::string file = "/tmp/anyfile.txt";
    MOCKER_CPP(IsPathValid, bool (*)(const std::filesystem::path &)).stubs().will(returnValue(false));

    EXPECT_EQ(ReadFileFirstLine(file), "");
}

TEST_F(TestRmrsResourceExport, ReadFileFirstLineReturnFileNotExist)
{
    std::string file = "/tmp/not_exist.txt";
    MOCKER_CPP(IsPathValid, bool (*)(const std::filesystem::path &)).stubs().will(returnValue(true));

    // canonical 会抛异常 → 走 catch → ""
    EXPECT_EQ(ReadFileFirstLine(file), "");
}

TEST_F(TestRmrsResourceExport, HandleMemInfoFileFailed)
{
    int numaid = 1;
    std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / "meminfo_test.txt";
    std::map<std::string, std::string> meminfo;
    ResourceExport resourceExport;
    auto res = resourceExport.HandleMemInfoFile(numaid, tmpFile, meminfo);
    EXPECT_EQ(res, false);
    if (std::filesystem::exists(tmpFile)) {
        std::filesystem::remove(tmpFile);
    }
}

TEST_F(TestRmrsResourceExport, HandleMemInfoFileSuccess)
{
    std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / "meminfo_test.txt";
    std::ofstream ofs(tmpFile);
    ofs << "Node 0 MemTotal: 1024 kB\n";
    std::map<std::string, std::string> meminfo;
    ResourceExport resourceExport;
    auto res = resourceExport.HandleMemInfoFile(0, tmpFile, meminfo);
    EXPECT_EQ(res, true);
    if (std::filesystem::exists(tmpFile)) {
        std::filesystem::remove(tmpFile);
    }
}

TEST_F(TestRmrsResourceExport, HandleMemInfoFileFailed1)
{
    std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / "meminfo_test.txt";
    std::ofstream ofs(tmpFile);
    ofs << "Node 0 OnlyTwoFields\n";
    std::map<std::string, std::string> meminfo;
    ResourceExport resourceExport;
    auto res = resourceExport.HandleMemInfoFile(0, tmpFile, meminfo);
    EXPECT_EQ(res, true);
    if (std::filesystem::exists(tmpFile)) {
        std::filesystem::remove(tmpFile);
    }
}

TEST_F(TestRmrsResourceExport, HandleMemInfoFileSuccess1)
{
    std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / "meminfo_test.txt";
    std::ofstream ofs(tmpFile);
    ofs << "MemTotal: 1024 kB\n";
    std::map<std::string, std::string> meminfo;
    ResourceExport resourceExport;
    auto res = resourceExport.HandleMemInfoFile(-1, tmpFile, meminfo);
    EXPECT_EQ(res, true);
    if (std::filesystem::exists(tmpFile)) {
        std::filesystem::remove(tmpFile);
    }
}

TEST_F(TestRmrsResourceExport, HandleMemInfoFileSuccess2)
{
    std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / "meminfo_test.txt";
    std::ofstream ofs(tmpFile);
    ofs << "InvalidLineWithoutValue\n";
    std::map<std::string, std::string> meminfo;
    ResourceExport resourceExport;
    auto res = resourceExport.HandleMemInfoFile(-1, tmpFile, meminfo);
    EXPECT_EQ(res, true);
    if (std::filesystem::exists(tmpFile)) {
        std::filesystem::remove(tmpFile);
    }
}

TEST_F(TestRmrsResourceExport, GetNumaIdToSocketIdFailed)
{
    MOCKER_CPP(ReadFileFirstLine, std::string(*)(const std::string &)).stubs().will(returnValue(std::string("")));
    ResourceExport resourceExport;
    auto res = resourceExport.GetNumaIdToSocketId(0);
    EXPECT_EQ(res, RMRS_ERROR_SIGN_INT);
}

TEST_F(TestRmrsResourceExport, GetNumaIdToSocketId_ParseCpuListEmpty)
{
    // case 3: ParseCpuList 返回空 vector
    MOCKER_CPP(ReadFileFirstLine, std::string(*)(const std::string &)).stubs().will(returnValue(std::string("0-3")));
    MOCKER_CPP(ParseCpuList, std::vector<int>(*)(const std::string &)).stubs().will(returnValue(std::vector<int>{}));

    ResourceExport resourceExport;
    auto res = resourceExport.GetNumaIdToSocketId(0);
    EXPECT_EQ(res, RMRS_ERROR_SIGN_INT);
}

TEST_F(TestRmrsResourceExport, GetNumaIdToSocketId_AllCpuFail)
{
    // case 5: ParseCpuList 返回非空，但所有 CPU 都返回 -1
    MOCKER_CPP(ReadFileFirstLine, std::string(*)(const std::string &)).stubs().will(returnValue(std::string("0-3")));
    MOCKER_CPP(ParseCpuList, std::vector<int>(*)(const std::string &))
        .stubs()
        .will(returnValue(std::vector<int>{0, 1}));
    MOCKER_CPP(ResourceExport::GetSocketIdFromCpu, int (*)(int)).stubs().will(returnValue(-1)); // 所有 CPU 都失败

    ResourceExport resourceExport;
    auto res = resourceExport.GetNumaIdToSocketId(0);
    EXPECT_EQ(res, RMRS_ERROR_SIGN_INT);
}

} // namespace rmrs::exports