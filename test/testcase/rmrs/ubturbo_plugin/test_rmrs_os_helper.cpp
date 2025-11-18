/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.‘
 */
#include <dirent.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <fstream>
#include <dirent.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "securec.h"
#define private public
#include "rmrs_file_util.h"
#include "rmrs_os_helper.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;
namespace rmrs {
using UuidNumaMapType = unordered_map<std::string, uint16_t> *;
using LocalNumaIdType = unordered_map<std::string, uint16_t> *;
using RemoteNumaIdType = unordered_map<std::string, uint16_t> *;
using LocalUsedMemType = unordered_map<std::string, uint64_t> *;
using RemoteUsedMemType = unordered_map<std::string, uint64_t> *;
using NumaLocalMapType = unordered_map<uint16_t, bool> *;
using UuidNameMapType = std::unordered_map<std::string, std::string> *;

using PidCreatTimeMapType = std::unordered_map<pid_t, time_t> *;
using VPidListType = std::vector<pid_t> *;
using UuidListType = std::vector<std::string> *;
using UuidPIDMapType = std::unordered_map<std::string, pid_t> *;
using PIDUuidMapType = std::unordered_map<pid_t, std::string> *;

std::unordered_map<std::string, uint16_t> uuidNumaMap{};
std::unordered_map<std::string, uint16_t> remoteNumaId{};
std::unordered_map<std::string, uint16_t> localNumaId{};
std::unordered_map<std::string, uint64_t> remoteUsedMem{};
std::unordered_map<std::string, uint64_t> localUsedMem{};
static std::unordered_map<uint16_t, bool> numaLocalMap{};
std::unordered_map<std::string, pid_t> uuidPIDMap{};
std::unordered_map<pid_t, std::string> pidUUIDMap{};
std::unordered_map<std::string, std::string> uuidNameMap{};
std::unordered_map<pid_t, time_t> pidCreatTimeMap{};
std::vector<pid_t> vPidList{};
std::vector<std::string> uuidList{};
const pid_t mockPid = 1234;
const time_t mockTimeStamp = 123456789;

UuidNameMapType MockGetUuidNameMap()
{
    uuidNameMap["uuid1"] = "name";
    return &uuidNameMap;
}

PidCreatTimeMapType MockGetPidCreatTimeMap()
{
    pidCreatTimeMap[mockPid] = mockTimeStamp;
    return &pidCreatTimeMap;
}
VPidListType MockGetVPidList()
{
    vPidList.push_back(mockPid);
    return &vPidList;
}
UuidListType MockGetUuidList()
{
    uuidList.push_back("uuid1");
    return &uuidList;
}
UuidPIDMapType MockGetUuidPIDMap()
{
    uuidPIDMap["uuid1"] = mockPid;
    return &uuidPIDMap;
}

PIDUuidMapType MockGetPIDUuidMap()
{
    pidUUIDMap[mockPid] = "uuid1";
    return &pidUUIDMap;
}

std::unordered_map<std::string, uint16_t> *MockGetUuidNumaMap()
{
    uuidNumaMap["uuid1"] = 0;
    return &uuidNumaMap;
}

std::unordered_map<std::string, uint16_t> *MockGetLocalNumaId()
{
    localNumaId["uuid1"] = 0;
    return &localNumaId;
}

std::unordered_map<std::string, uint16_t> *MockGetRemoteNumaId()
{
    remoteNumaId["uuid1"] = 0;
    return &remoteNumaId;
}

std::unordered_map<std::string, uint64_t> *MockGetLocalUsedMem()
{
    localUsedMem["uuid1"] = 0;
    return &localUsedMem;
}

std::unordered_map<std::string, uint64_t> *MockGetRemoteUsedMem()
{
    remoteUsedMem["uuid1"] = 0;
    return &remoteUsedMem;
}

std::unordered_map<uint16_t, bool> *MockGetNumaLocalMap()
{
    const int mockRemoteNumaId = 4;
    numaLocalMap[0] = true;
    numaLocalMap[1] = true;
    numaLocalMap[mockRemoteNumaId] = false;
    return &numaLocalMap;
}

std::unordered_map<uint16_t, bool> *MockGetNumaLocalMap_false()
{
    numaLocalMap[0] = true;
    numaLocalMap[1] = false;
    return &numaLocalMap;
}

class TestRmrsOsHelper : public ::testing::Test {
protected:
    TestRmrsOsHelper() {}
    virtual void SetUp()
    {
        GlobalMockObject::reset();
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }
};

TEST(TestRmrsOsHelper, GetNumaCPUInfosSuccessWithRightData)
{
    unordered_map<uint16_t, uint16_t> cpuSocketMap;
    unordered_map<uint16_t, uint16_t> cpuNumaMap;
    unordered_map<uint16_t, bool> numaLocalMap;
    std::vector<NumaInfo> numaInfos;
    std::string hostName = "computer1";
    std::string nodeId = "Node0";
    OsHelper osHelper;
    auto result = osHelper.GetNumaCPUInfos(cpuSocketMap, cpuNumaMap, numaLocalMap, numaInfos);
    EXPECT_EQ(result, RMRS_OK);
}

TEST(TestRmrsOsHelper, GetNumaCPUInfosFailedWithGetFileContentError)
{
    unordered_map<uint16_t, uint16_t> cpuSocketMap;
    unordered_map<uint16_t, uint16_t> cpuNumaMap;
    unordered_map<uint16_t, bool> numaLocalMap;
    std::vector<NumaInfo> numaInfos;
    std::string hostName = "computer1";
    std::string nodeId = "Node0";
    OsHelper osHelper;
    MOCKER_CPP(&OsHelper::getFileContent,
               RmrsResult(*)(const std::string &filePath, std::vector<std::string> &info, const std::string &fileName))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    auto result = osHelper.GetNumaCPUInfos(cpuSocketMap, cpuNumaMap, numaLocalMap, numaInfos);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, GetNumaCPUInfosFailedWithGetMemInfoErrorNoent)
{
    unordered_map<uint16_t, uint16_t> cpuSocketMap;
    unordered_map<uint16_t, uint16_t> cpuNumaMap;
    unordered_map<uint16_t, bool> numaLocalMap;
    std::vector<NumaInfo> numaInfos;
    std::string hostName = "computer1";
    std::string nodeId = "Node0";
    OsHelper osHelper;
    MOCKER_CPP(&OsHelper::getMemInfo, RmrsResult(*)(const std::string &nodePath, NumaInfo &tempNumaInfo))
        .stubs()
        .will(returnValue(RMRS_ERROR_NOENT));
    auto result = osHelper.GetNumaCPUInfos(cpuSocketMap, cpuNumaMap, numaLocalMap, numaInfos);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, GetNumaCPUInfosFailedWithGetMemInfoError)
{
    unordered_map<uint16_t, uint16_t> cpuSocketMap;
    unordered_map<uint16_t, uint16_t> cpuNumaMap;
    unordered_map<uint16_t, bool> numaLocalMap;
    std::vector<NumaInfo> numaInfos;
    std::string hostName = "computer1";
    std::string nodeId = "Node0";
    OsHelper osHelper;
    MOCKER_CPP(&OsHelper::getMemInfo, RmrsResult(*)(const std::string &nodePath, NumaInfo &tempNumaInfo))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    auto result = osHelper.GetNumaCPUInfos(cpuSocketMap, cpuNumaMap, numaLocalMap, numaInfos);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, UpdateNumaMemInfoFailedWithUpdateMemInfoErrorNoent)
{
    ResourceExport *resourceCollect;
    resourceCollect = new ResourceExport();
    OsHelper osHelper;
    MOCKER_CPP(&OsHelper::updateMemInfo,
               RmrsResult(*)(const std::string &nodePath, ResourceExport *resourceCollect, std::string folderName))
        .stubs()
        .will(returnValue(RMRS_ERROR_NOENT));
    auto result = osHelper.UpdateNumaMemInfo(resourceCollect);
    delete resourceCollect;
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, UpdateNumaMemInfoFailedWithUpdateMemInfoError)
{
    ResourceExport *resourceCollect;
    resourceCollect = new ResourceExport();
    OsHelper osHelper;
    MOCKER_CPP(&OsHelper::updateMemInfo,
               RmrsResult(*)(const std::string &nodePath, ResourceExport *resourceCollect, std::string folderName))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    auto result = osHelper.UpdateNumaMemInfo(resourceCollect);
    delete resourceCollect;
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, getMemInfoFailedWithGetFileContentError)
{
    const std::string nodePath = "";
    NumaInfo tempNumaInfo;
    MOCKER_CPP(&OsHelper::getFileContent,
               RmrsResult(*)(const std::string &filePath, std::vector<std::string> &info, const std::string &fileName))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    auto result = OsHelper::getMemInfo(nodePath, tempNumaInfo);
    EXPECT_EQ(result, RMRS_ERROR_NOENT);
}

TEST(TestRmrsOsHelper, getMemInfoFailedWithCheckFileContentError)
{
    const std::string nodePath = "";
    NumaInfo tempNumaInfo;
    MOCKER_CPP(&OsHelper::getFileContent,
               RmrsResult(*)(const std::string &filePath, std::vector<std::string> &info, const std::string &fileName))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&OsHelper::checkFileContent,
               RmrsResult(*)(std::vector<std::string> & info, const std::string &fileName, int vectorNum))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    auto result = OsHelper::getMemInfo(nodePath, tempNumaInfo);
    EXPECT_EQ(result, RMRS_ERROR_NOENT);
}

TEST(TestRmrsOsHelper, GetVMUsedMemorySuccessWithRightData)
{
    ResourceExport *resourceCollect;
    resourceCollect = new ResourceExport();
    auto result = OsHelper::GetVMUsedMemory(resourceCollect);
    delete resourceCollect;
    EXPECT_EQ(result, RMRS_OK);
}

TEST(TestRmrsOsHelper, GetInfoFromNumaMapsFailedWithVoidData)
{
    ResourceExport *resourceCollect;
    resourceCollect = new ResourceExport();
    string uuid = "uuid";
    string path = "path";
    auto result = OsHelper::GetInfoFromNumaMaps(uuid, path, resourceCollect);
    delete resourceCollect;
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, GetInfoFromNumaMapsWithWrongData)
{
    ResourceExport *resourceCollect;
    resourceCollect = new ResourceExport();
    string uuid = "uuid";
    string path = "/sys/devices/system/node/node0/cpulist";
    auto result = OsHelper::GetInfoFromNumaMaps(uuid, path, resourceCollect);
    delete resourceCollect;
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, ReadCpuInfoWithVoidNodeInfo)
{
    vector<std::string> nodeInfo = {""};
    NumaInfo tempNumaInfo;
    unordered_map<uint16_t, uint16_t> cpuSocketMap;
    std::string folderName = "node63";
    auto result = OsHelper::ReadCpuInfo(nodeInfo, tempNumaInfo, cpuSocketMap, folderName);
    EXPECT_EQ(result, 63);
}

TEST(TestRmrsOsHelper, CheckFileContentFailedWithInvalidContent)
{
    vector<std::string> mockInfo = {};
    const std::string fileName = "mock.cpp";
    int vectorNum = -55;
    auto result = OsHelper::checkFileContent(mockInfo, fileName, vectorNum);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, GetFileContentFailedWithInvalidContent)
{
    const std::string filePath = "";
    std::vector<std::string> info;
    const std::string fileName = "mock.cpp";
    auto result = OsHelper::getFileContent(filePath, info, fileName);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, GetRemoteAvailableFlagWithHcomNuma)
{
    vector<NumaInfo> numaInfos;
    NumaInfo tmpNumaInfo;
    tmpNumaInfo.numaMetaInfo.numaId = 63;
    tmpNumaInfo.numaMetaInfo.isLocal = false;
    numaInfos.push_back(tmpNumaInfo);
    auto result = OsHelper::GetRemoteAvailableFlag(numaInfos);
    EXPECT_EQ(result, RMRS_OK);
}

TEST(TestRmrsOsHelper, GetPidCreatTimeSuccess)
{
    ResourceExport *resourceCollect;
    resourceCollect = new ResourceExport();
    MOCKER_CPP(&ResourceExport::GetPidCreatTimeMap, PidCreatTimeMapType(*)())
        .stubs()
        .will(invoke(MockGetPidCreatTimeMap));
    MOCKER_CPP(&ResourceExport::GetVPidList, VPidListType(*)()).stubs().will(invoke(MockGetVPidList));
    MOCKER_CPP(&ResourceExport::GetUuidList, UuidListType(*)()).stubs().will(invoke(MockGetUuidList));
    MOCKER_CPP(&ResourceExport::GetUuidPIDMap, UuidPIDMapType(*)()).stubs().will(invoke(MockGetUuidPIDMap));

    auto result = OsHelper::GetPidCreatTime(resourceCollect);
    delete resourceCollect;
    EXPECT_EQ(result, RMRS_OK);
}

TEST(TestRmrsOsHelper, GetSocketCpuRelationSuccessWithMockData)
{
    // 模拟文件路径和socket信息
    string mockDir = "../";
    string cpuSocketPathPrefix = "/sys/devices/system/cpu";
    string cpuSocketPath = "/topology/physical_package_id";

    auto dir = opendir(mockDir.c_str());
    MOCKER(opendir).stubs().will(returnValue(dir));
    closedir(dir);

    dirent *entry = new dirent;
    // 假设我们要模拟一个 "cpu0" 的目录
    const char *cpuDirName = "cpu0";
    strncpy_s(entry->d_name, sizeof(entry->d_name), cpuDirName, strlen(cpuDirName));
    entry->d_name[sizeof(entry->d_name) - 1] = '\0'; // 确保字符串以 null 结束
    MOCKER(readdir).stubs().will(returnValue(entry));
    MOCKER(RmrsFileUtil::IsSpecifiedPath).stubs().will(returnValue(RMRS_OK));

    stringstream socketPath;
    vector<string> socketInfo = {"1", "2"};
    MOCKER(RmrsFileUtil::GetFileInfo)
        .stubs()
        .with(outBound(socketPath.str()), outBound(socketInfo))
        .will(returnValue(RMRS_OK));
    MOCKER(closedir).stubs().will(returnValue(0));

    // 测试函数
    unordered_map<uint16_t, uint16_t> cpuSocketMap;
    OsHelper osHelper;
    auto result = osHelper.GetSocketCpuRelation(cpuSocketMap);
    delete entry;
    // 断言测试结果
    EXPECT_EQ(result, RMRS_OK);
}

TEST(TestRmrsOsHelper, GetPidFromUUIDSuccess)
{
    ResourceExport *resourceCollect;
    resourceCollect = new ResourceExport();
    MOCKER_CPP(&ResourceExport::GetUuidPIDMap, UuidPIDMapType(*)()).stubs().will(invoke(MockGetUuidPIDMap));
    MOCKER_CPP(&ResourceExport::GetPidUUIDMap, PIDUuidMapType(*)()).stubs().will(invoke(MockGetPIDUuidMap));
    MOCKER_CPP(&ResourceExport::GetUuidNameMap, UuidNameMapType(*)()).stubs().will(invoke(MockGetUuidNameMap));
    MOCKER_CPP(&ResourceExport::GetUuidList, UuidListType(*)()).stubs().will(invoke(MockGetUuidList));
    auto result = OsHelper::GetPidFromUUID(resourceCollect);
    delete resourceCollect;
    EXPECT_EQ(result, RMRS_OK);
}

TEST(TestRmrsOsHelper, GetPidByVmNameSuccessWithMockData)
{
    stringstream ssPath;
    string name = "mock_vm_1";
    pid_t pid = 1;
    std::string path = "/var/run/libvirt/qemu/mock_vm_1.pid";
    std::ofstream ofs(path);
    ofs << "1";
    ofs.close();
    vector<string> pidInfo = {"1"};
    MOCKER(RmrsFileUtil::GetFileInfo)
        .stubs()
        .with(outBound(ssPath.str()), outBound(pidInfo))
        .will(returnValue(RMRS_OK));
    auto result = OsHelper::GetPidByVmName(name);
    EXPECT_EQ(result, pid);
}

TEST(TestRmrsOsHelper, GetPidFromUUIDSuccessWithWrongVmName)
{
    ResourceExport *resourceCollect;
    resourceCollect = new ResourceExport();
    MOCKER_CPP(&ResourceExport::GetUuidPIDMap, UuidPIDMapType(*)()).stubs().will(invoke(MockGetUuidPIDMap));
    MOCKER_CPP(&ResourceExport::GetPidUUIDMap, PIDUuidMapType(*)()).stubs().will(invoke(MockGetPIDUuidMap));
    MOCKER_CPP(&ResourceExport::GetUuidNameMap, UuidNameMapType(*)()).stubs().will(invoke(MockGetUuidNameMap));
    MOCKER_CPP(&ResourceExport::GetUuidList, UuidListType(*)()).stubs().will(invoke(MockGetUuidList));
    MOCKER_CPP(&OsHelper::GetPidByVmName, pid_t(*)(const string &name)).stubs().will(returnValue(-1));
    auto result = OsHelper::GetPidFromUUID(resourceCollect);
    delete resourceCollect;
    EXPECT_EQ(result, RMRS_OK);
}

// 在测试开始时，用模拟的 GetFileInfo 替换全局函数
TEST(TestRmrsOsHelper, GetProcessStartTimeSuccesswithMockData)
{
    stringstream ssPath;
    vector<string> info = {"abc", "def"};
    MOCKER(RmrsFileUtil::GetFileInfo).stubs().with(outBound(ssPath.str()), outBound(info)).will(returnValue(RMRS_OK));
    // 准备变量
    pid_t pid = 1234; // 假设的进程ID
    time_t timestamp;
    // 创建 OsHelper 实例
    OsHelper osHelper;
    // 调用函数
    uint32_t result = osHelper.GetProcessStartTime(pid, timestamp);
    // 验证返回结果
    EXPECT_EQ(result, RMRS_OK);
}

TEST(TestRmrsOsHelper, GetFileContentShouldReturnErrorWhenGetFileInfoFailed)
{
    stringstream ssPath;
    vector<string> info = {"abc", "def"};
    MOCKER(RmrsFileUtil::GetFileInfo)
        .stubs()
        .with(outBound(ssPath.str()), outBound(info))
        .will(returnValue(RMRS_ERROR));

    std::string filePath = "/test";
    std::string fileName;
    uint32_t result = OsHelper::getFileContent(filePath, info, fileName);

    EXPECT_EQ(result, RMRS_ERROR);
}

TEST(TestRmrsOsHelper, GetVmsPidOnNumaShouldReturnOk)
{
    ResourceExport resourceCollect;

    resourceCollect.localUsedMem["node1"] = 1024;
    resourceCollect.remoteUsedMem["node1"] = 1024;
    
    uint32_t result = OsHelper::GetVmsPidOnNuma(&resourceCollect);

    EXPECT_EQ(result, RMRS_OK);
}

TEST(TestRmrsOsHelper, GetPidByVmNameShouldFailedWhenOpendirReturnNullptr)
{
    MOCKER_CPP(opendir, DIR* (*)(const char*))
        .stubs()
        .will(returnValue((DIR*)nullptr));
    std::string name = "/test";
    pid_t result = OsHelper::GetPidByVmName(name);

    EXPECT_EQ(result, -1);
}

TEST(TestRmrsOsHelper, GetPidByVmNameShouldFailedWhenGetFileInfoFailed)
{
    MOCKER_CPP(opendir, DIR * (*)(const char *)).stubs().will(returnValue((DIR *)0x1));

    stringstream socketPath;
    vector<string> socketInfo = {"1", "2"};
    MOCKER(RmrsFileUtil::GetFileInfo)
        .stubs()
        .with(outBound(socketPath.str()), outBound(socketInfo))
        .will(returnValue(RMRS_ERROR));
    std::string name = "/test";
    pid_t result = OsHelper::GetPidByVmName(name);

    EXPECT_EQ(result, -1);
}

TEST(TestRmrsOsHelper, GetVMUsedMemoryShouldReturnOk)
{
    MOCKER_CPP(rmrs::OsHelper::GetInfoFromNumaMaps, uint32_t (*)(const std::string &uuid, const std::string &path,
                                       ResourceExport *resourceCollect))
        .stubs()
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(rmrs::OsHelper::GetVmPageSizeFromNumaMaps,
               RmrsResult(*)(const std::string &uuid, const std::string &path, ResourceExport *resourceCollect))
        .stubs()
        .will(returnValue(RMRS_OK));

    ResourceExport resourceCollect;
    pid_t pid = 1;
    resourceCollect.vPidList.push_back(pid);
    
    uint32_t result = OsHelper::GetVMUsedMemory(&resourceCollect);
    EXPECT_EQ(result, RMRS_OK);
}
} // namespace rmrs