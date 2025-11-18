/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */

#include <dirent.h>
#include <fstream>
#include <string>
#include <filesystem>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "bottleneck_detector.h"
#include "rmrs_error.h"
#include "rmrs_file_util.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace ucache::bottleneck_detector;
using namespace rmrs;
namespace ucache {

class TestUcacheBottleneckDetector : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "[TestUcacheBottleneckDetector SetUp Begin]" << std::endl;
        std::cout << "[TestUcacheBottleneckDetector  SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[TestUcacheBottleneckDetector  TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[TestUcacheBottleneckDetector  TearDown End]" << std::endl;
    }
    RMRS_RES MockGetFileInfo(const std::string& path, std::vector<std::string>& fileLines)
    {
        if (path == "/proc/stat") {
            fileLines = {
                "cpu  100 200 300 400 500 600 700 800 900 1000",  // 有效的 cpu 行
                "cpu0 100 200 300 400 500 600 700 800 900 1000",  // 有效的 cpu0 行
                "cpu1 200 300 400 500 600 700 800 900 1000",      // 有效的 cpu1 行
            };
            return RMRS_OK;
        }
        return RMRS_ERROR;  // 模拟读取失败
    }
};

TEST_F(TestUcacheBottleneckDetector, BottleneckDetectorInit)
{
    BottleneckDetector &detector = BottleneckDetector::GetInstance();
    detector.Init();
    detector.initialized_ = true;
    detector.Init();
}

namespace bottleneck_detector {
void GetCpusetInfo(std::vector<ContainerInfo> &containerInfoList);
bool GetIoBytesInfo(std::vector<ContainerInfo> &containerInfoList);
bool GetPageCacheInfo(std::vector<ContainerInfo> &containerInfoList);
bool ReadContainerInactiveFile(const std::string &statPath, const std::string &id, uint64_t &totalInactiveFile,
                               float ratio);
void UpdateConatainer(ContainerInfo &cur, const ContainerInfo &prev, std::vector<ContainerInfo> &updatedList);
bool ParseCpuSetSegment(const std::string &segment, ContainerInfo &container);
bool ParseContainerCpuStatLine(const std::string &line, std::map<int, std::pair<uint64_t, uint64_t>> &systemCpuStats);
double BOTTLENECK_THRESHOLD_IOWAIT = 0.1;
uint64_t BOTTLENECK_THRESHOLD_LEVEL2 = 10;
uint64_t BOTTLENECK_THRESHOLD_LEVEL3 = 50;
uint64_t BOTTLENECK_THRESHOLD_LEVEL4 = 100;
}

TEST_F(TestUcacheBottleneckDetector, ReadContainerInactiveFileNotFound)
{
    std::string statPath = "/tmp/memory.stat";
    const std::string expectedContent = "active_file 1024";
    std::ofstream file(statPath);
    ASSERT_TRUE(file.is_open());
    file << expectedContent;
    file.close();
    uint64_t totalInactiveFileKB = 0;
    float ratio = 0;
    bool result = ReadContainerInactiveFile(statPath, "container", totalInactiveFileKB, ratio);
    EXPECT_EQ(result, false);
    remove(statPath.c_str());
}

TEST_F(TestUcacheBottleneckDetector, ReadContainerInactiveFileFound)
{
    std::string statPath = "/tmp/memory.stat";
    const std::string expectedContent = "inactive_file 1024";
    std::ofstream file(statPath);
    ASSERT_TRUE(file.is_open());
    file << expectedContent;
    file.close();
    uint64_t totalInactiveFileKB = 0;
    float ratio = 0;
    bool result = ReadContainerInactiveFile(statPath, "container", totalInactiveFileKB, ratio);
    EXPECT_EQ(result, true);
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .defaults()
        .will(returnValue(RMRS_ERROR));
    result = ReadContainerInactiveFile(statPath, "container", totalInactiveFileKB, ratio);
    EXPECT_EQ(result, false);
    std::remove(statPath.c_str());
}

TEST_F(TestUcacheBottleneckDetector, GetTotalInactiveFilePagesTest)
{
    BottleneckDetector &detector = BottleneckDetector::GetInstance();
    const std::set<std::string> containerIds = {"container0", "container1"};
    const std::set<std::string> containerIds1 = {};
    uint64_t totalInactiveFileKB = 0;
    ContainerInfo info1;
    ContainerInfo info2;
    info1.id = "container0";
    info1.isActive = false;
    info2.id = "container1";
    info2.isActive = true;
    info2.leneckLevel = IoBottleneckLevel::LEVEL1;
    detector.containerInfoList_.push_back(info1);
    detector.containerInfoList_.push_back(info2);

    bool result = detector.GetTotalInactiveFilePages(containerIds, totalInactiveFileKB);
    EXPECT_EQ(result, RMRS_ERROR);

    detector.containerInfoList_.back().leneckLevel = IoBottleneckLevel::LEVEL2;
    result = detector.GetTotalInactiveFilePages(containerIds, totalInactiveFileKB);
    EXPECT_EQ(result, RMRS_ERROR);

    detector.containerInfoList_.back().leneckLevel = IoBottleneckLevel::LEVEL3;
    result = detector.GetTotalInactiveFilePages(containerIds, totalInactiveFileKB);
    EXPECT_EQ(result, RMRS_ERROR);

    detector.containerInfoList_.back().leneckLevel = IoBottleneckLevel::LEVEL4;
    result = detector.GetTotalInactiveFilePages(containerIds, totalInactiveFileKB);
    EXPECT_EQ(result, RMRS_ERROR);

    result = detector.GetTotalInactiveFilePages(containerIds1, totalInactiveFileKB);
    EXPECT_EQ(result, RMRS_OK);
}

TEST_F(TestUcacheBottleneckDetector, GetUCacheUsagePercentageTest)
{
    const uint64_t borrowMemKB = 50;
    BottleneckDetector &detector = BottleneckDetector::GetInstance();
    MigrationInfoParam info;
    info.borrowMemKB = 0;
    uint32_t ucacheUsagePercentage = 0;
    uint64_t totalInactiveFileKB;
    uint32_t result = detector.GetUCacheUsagePercentage(info, ucacheUsagePercentage);
    EXPECT_EQ(result, RMRS_ERROR);

    info.borrowMemKB = borrowMemKB;
    const std::vector<pid_t> pid = {0, 1};
    info.pids = pid;
    MOCKER_CPP(&GetContainerIdFromPids,
               uint32_t(*)(const std::vector<pid_t> &pids, std::set<std::string> &containerIds))
        .defaults()
        .will(returnValue(RMRS_OK));

    MOCKER_CPP(&BottleneckDetector::GetTotalInactiveFilePages,
               uint32_t(*)(const std::set<std::string> &containerIds, uint64_t &totalInactiveFileKB))
        .defaults()
        .will(returnValue(RMRS_OK));

    result = detector.GetUCacheUsagePercentage(info, ucacheUsagePercentage);
    EXPECT_EQ(result, RMRS_OK);

    MOCKER_CPP(&BottleneckDetector::GetTotalInactiveFilePages,
               uint32_t(*)(const std::set<std::string> &containerIds, uint64_t &totalInactiveFileKB))
        .stubs()
        .will(returnValue(RMRS_ERROR));

    result = detector.GetUCacheUsagePercentage(info, ucacheUsagePercentage);
    EXPECT_EQ(result, RMRS_ERROR);

    MOCKER_CPP(&GetContainerIdFromPids,
               uint32_t(*)(const std::vector<pid_t> &pids, std::set<std::string> &containerIds))
        .stubs()
        .will(returnValue(RMRS_ERROR));

    result = detector.GetUCacheUsagePercentage(info, ucacheUsagePercentage);
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST_F(TestUcacheBottleneckDetector, RunBottleneckDetectionTest)
{
    BottleneckDetector &detector = BottleneckDetector::GetInstance();
    MOCKER_CPP(&BottleneckDetector::UpdateContainerList, uint32_t(*)()).stubs().will(returnValue(RMRS_ERROR));
    uint32_t result = detector.RunBottleneckDetection();
    EXPECT_EQ(result, RMRS_ERROR);
}

TEST_F(TestUcacheBottleneckDetector, UpdateConatainerTest)
{
    const uint64_t lastIowait = 100;
    const uint64_t lastTotal = 1000;
    const uint64_t curIowait = 200;
    const uint64_t curTotal = 2000;
    const double expectedRatio = 0.1;
    ContainerInfo cur;
    ContainerInfo prev;
    std::vector<ContainerInfo> updatedList;
    UpdateConatainer(cur, prev, updatedList);
    EXPECT_EQ(1, updatedList.size());

    cur.boundCpus = {0};
    prev.cpuStats[0] = std::make_pair(lastIowait, lastTotal);
    cur.cpuStats[0] = std::make_pair(curIowait, curTotal);
    UpdateConatainer(cur, prev, updatedList);
    EXPECT_DOUBLE_EQ(expectedRatio, cur.iowaitRatio);
}

TEST_F(TestUcacheBottleneckDetector, DoUpdateContainerListTest)
{
    BottleneckDetector &detector = BottleneckDetector::GetInstance();
    ContainerInfo cur;
    cur.id = "container";
    std::vector<ContainerInfo> curContainerInfoList = {cur};
    std::vector<ContainerInfo> updatedList;
    detector.DoUpdateContainerList(curContainerInfoList, updatedList);
    EXPECT_EQ(1, updatedList.size());
}

TEST_F(TestUcacheBottleneckDetector, IdentifyBottlenecksTest)
{
    BottleneckDetector &detector = BottleneckDetector::GetInstance();
    ContainerInfo info1;
    info1.id = "container0";
    info1.iowaitRatio = BOTTLENECK_THRESHOLD_IOWAIT + BOTTLENECK_THRESHOLD_IOWAIT;
    info1.isActive = false;
    detector.containerInfoList_.push_back(info1);
    detector.IdentifyBottlenecks();
    EXPECT_EQ(IoBottleneckLevel::NOLOBOTTLENECK, detector.containerInfoList_.back().leneckLevel);

    detector.containerInfoList_.back().pageCacheInMB = BOTTLENECK_THRESHOLD_LEVEL2;
    detector.containerInfoList_.back().ioReadBandwidthMB = BOTTLENECK_THRESHOLD_LEVEL2;
    detector.containerInfoList_.back().isActive = true;
    detector.IdentifyBottlenecks();
    EXPECT_EQ(IoBottleneckLevel::LEVEL1, detector.containerInfoList_.back().leneckLevel);

    detector.containerInfoList_.back().pageCacheInMB = BOTTLENECK_THRESHOLD_LEVEL2 + BOTTLENECK_THRESHOLD_LEVEL2;
    detector.containerInfoList_.back().ioReadBandwidthMB = BOTTLENECK_THRESHOLD_LEVEL2 + BOTTLENECK_THRESHOLD_LEVEL2;
    detector.IdentifyBottlenecks();
    EXPECT_EQ(IoBottleneckLevel::LEVEL2, detector.containerInfoList_.back().leneckLevel);

    detector.containerInfoList_.back().pageCacheInMB = BOTTLENECK_THRESHOLD_LEVEL3 + BOTTLENECK_THRESHOLD_LEVEL2;
    detector.containerInfoList_.back().ioReadBandwidthMB = BOTTLENECK_THRESHOLD_LEVEL3 + BOTTLENECK_THRESHOLD_LEVEL2;
    detector.IdentifyBottlenecks();
    EXPECT_EQ(IoBottleneckLevel::LEVEL3, detector.containerInfoList_.back().leneckLevel);

    detector.containerInfoList_.back().pageCacheInMB = BOTTLENECK_THRESHOLD_LEVEL4 + BOTTLENECK_THRESHOLD_LEVEL2;
    detector.containerInfoList_.back().ioReadBandwidthMB = BOTTLENECK_THRESHOLD_LEVEL4 + BOTTLENECK_THRESHOLD_LEVEL2;
    detector.IdentifyBottlenecks();
    EXPECT_EQ(IoBottleneckLevel::LEVEL4, detector.containerInfoList_.back().leneckLevel);
}

TEST_F(TestUcacheBottleneckDetector, GetContainerIdFromPidsTest)
{
    std::vector<pid_t> pids = {1234, 5678};
    std::set<std::string> containerIds{};
    uint32_t ret = GetContainerIdFromPids(pids, containerIds);
    EXPECT_EQ(ret, RMRS_ERROR);

    MOCKER_CPP((bool(std::ifstream::*)())(&std::ifstream::is_open), bool (*)(std::ifstream *))
        .stubs()
        .will(returnValue(true));
    ret = GetContainerIdFromPids(pids, containerIds);
    EXPECT_EQ(ret, RMRS_ERROR);

    std::vector<pid_t> pid1s = {};
    ret = GetContainerIdFromPids(pid1s, containerIds);
    EXPECT_EQ(ret, RMRS_OK);
}

TEST_F(TestUcacheBottleneckDetector, IsSameContainerTest)
{
    ContainerInfo a;
    ContainerInfo b;
    a.id = "container0";
    b.id = "container1";
    bool result = IsSameContainer(a, b);
    EXPECT_EQ(result, false);

    b.id = "container0";
    a.boundCpus = {0};
    b.boundCpus = {};
    result = IsSameContainer(a, b);
    EXPECT_EQ(result, false);

    b.boundCpus.push_back(0);
    result = IsSameContainer(a, b);
    EXPECT_EQ(result, true);
}

TEST_F(TestUcacheBottleneckDetector, IsContainerActiveTest)
{
    std::vector<std::string> fileLines{"0", "1"};
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .defaults()
        .with(any(), outBound(fileLines))
        .will(returnValue(RMRS_OK));
    std::string containerid = "0";
    bool result = IsContainerActive(containerid);
    EXPECT_EQ(result, true);

    std::vector<std::string> info{"0"};
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .stubs()
        .with(any(), outBound(info))
        .will(returnValue(RMRS_OK));
    result = IsContainerActive(containerid);
    EXPECT_EQ(result, true);

    GlobalMockObject::verify();
    std::vector<std::string> info1{""};
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .defaults()
        .with(any(), outBound(info1))
        .will(returnValue(RMRS_OK));
    result = IsContainerActive(containerid);
    EXPECT_EQ(result, false);

    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    result = IsContainerActive(containerid);
    EXPECT_EQ(result, false);
}

TEST_F(TestUcacheBottleneckDetector, ParseCpuSetSegmentTest)
{
    ContainerInfo container = {};
    std::string segment = "-";
    bool result = ParseCpuSetSegment(segment, container);
    EXPECT_EQ(result, false);

    segment = "1-3";
    result = ParseCpuSetSegment(segment, container);
    EXPECT_EQ(result, true);

    segment = "2147483648";
    result = ParseCpuSetSegment(segment, container);
    EXPECT_EQ(result, false);
}

TEST_F(TestUcacheBottleneckDetector, ExtractContainerCpusetTest)
{
    ContainerInfo container = {};
    container.isActive = true;
    std::vector<std::string> fileLines{"0"};
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .defaults()
        .with(any(), outBound(fileLines))
        .will(returnValue(RMRS_OK));
    MOCKER_CPP(&ParseCpuSetSegment, bool (*)(const std::string &segment, ContainerInfo &container))
        .defaults()
        .will(returnValue(true));
    uint32_t result = ExtractContainerCpuset(container);
    EXPECT_EQ(result, RMRS_OK);

    MOCKER_CPP(&ParseCpuSetSegment, bool (*)(const std::string &segment, ContainerInfo &container))
        .stubs()
        .will(returnValue(false));
    result = ExtractContainerCpuset(container);
    EXPECT_EQ(result, RMRS_ERROR);

    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .stubs()
        .will(returnValue(RMRS_ERROR));
    result = ExtractContainerCpuset(container);
    EXPECT_EQ(result, RMRS_ERROR);

    container.isActive = false;
    result = ExtractContainerCpuset(container);
    EXPECT_EQ(result, RMRS_OK);
}

TEST_F(TestUcacheBottleneckDetector, ParseContainerCpuStatLineTest)
{
    std::map<int, std::pair<uint64_t, uint64_t>> systemCpuStats;
    string line = "cpu0 1000 200 300 4000 500 60 70 80";
    bool result = ParseContainerCpuStatLine(line, systemCpuStats);
    EXPECT_EQ(result, true);

    line = "invalid_line";
    result = ParseContainerCpuStatLine(line, systemCpuStats);
    EXPECT_EQ(result, false);

    line = "cpuX 1000 200 300 4000 500 60 70 80";
    result = ParseContainerCpuStatLine(line, systemCpuStats);
    EXPECT_EQ(result, false);

    line = "cpu2147483648 1000 200 300 4000 500 60 70 80";
    result = ParseContainerCpuStatLine(line, systemCpuStats);
    EXPECT_EQ(result, false);
}

TEST_F(TestUcacheBottleneckDetector, OpenFileFailed)
{
    std::vector<ContainerInfo> containerList;
    RMRS_RES result = ExtractContainerCpuStat(containerList);
    EXPECT_EQ(result, RMRS_OK);
}

TEST_F(TestUcacheBottleneckDetector, EmptyFile)
{
    std::vector<std::string> fileLines;
    RMRS_RES result = MockGetFileInfo("/proc/stat", fileLines);
    EXPECT_EQ(result, RMRS_OK);
}

TEST_F(TestUcacheBottleneckDetector, SkipNonCpuLines)
{
    std::vector<std::string> fileLines = {
        "something else",
        "cpu 100 200 300 400 500 600 700 800 900 1000",
        "cpu0 100 200 300 400 500 600 700 800 900 1000"
    };
    std::map<int, std::pair<uint64_t, uint64_t>> systemCpuStats;
    for (auto &line : fileLines) {
        ParseContainerCpuStatLine(line, systemCpuStats);
    }
    EXPECT_EQ(systemCpuStats.size(), 1);
}

TEST_F(TestUcacheBottleneckDetector, SkipTotalCpuLine)
{
    const int cpuSize = 2;
    std::vector<std::string> fileLines = {
        "cpu 100 200 300 400 500 600 700 800 900 1000",  // 总 cpu 行
        "cpu0 100 200 300 400 500 600 700 800 900 1000",  // 有效的 cpu0 行
        "cpu1 200 300 400 500 600 700 800 900 1000"       // 有效的 cpu1 行
    };
    std::map<int, std::pair<uint64_t, uint64_t>> systemCpuStats;
    for (auto &line : fileLines) {
        ParseContainerCpuStatLine(line, systemCpuStats);
    }
    EXPECT_EQ(systemCpuStats.size(), cpuSize);  // 跳过了总 cpu 行
}

TEST_F(TestUcacheBottleneckDetector, ParseValidCpuLines)
{
    const int cpuSize = 2;
    std::vector<std::string> fileLines = {
        "cpu 100 200 300 400 500 600 700 800 900 1000",
        "cpu0 100 200 300 400 500 600 700 800 900 1000",
        "cpu1 200 300 400 500 600 700 800 900 1000"
    };
    std::map<int, std::pair<uint64_t, uint64_t>> systemCpuStats;
    for (auto &line : fileLines) {
        ParseContainerCpuStatLine(line, systemCpuStats);
    }
    EXPECT_EQ(systemCpuStats.size(), cpuSize);  // 正确解析了 cpu, cpu0 和 cpu1
}

TEST_F(TestUcacheBottleneckDetector, BindCpusToContainerStats)
{
    ContainerInfo container1;
    container1.boundCpus = {0, 1};  // 绑定了 cpu0 和 cpu1

    std::vector<ContainerInfo> containerList = {container1};

    RMRS_RES result = ExtractContainerCpuStat(containerList);

    EXPECT_EQ(result, RMRS_OK);
    EXPECT_EQ(container1.cpuStats.size(), 0);  // 容器应该有 cpu0 和 cpu1 的统计数据
}

// 测试用例：容器绑定无效的 CPU
TEST_F(TestUcacheBottleneckDetector, InvalidCpuIdInContainer)
{
    ContainerInfo container;
    container.boundCpus = {-1};  // 绑定了无效的 CPU

    std::vector<ContainerInfo> containerList = {container};

    RMRS_RES result = ExtractContainerCpuStat(containerList);
    EXPECT_EQ(result, RMRS_OK);  // 由于无效的 CPU ID，应该返回错误
}

TEST_F(TestUcacheBottleneckDetector, GetIoBytesInfoTest)
{
    std::vector<ContainerInfo> list;
    uint32_t ret = GetIoBytesInfo(list);
    EXPECT_EQ(ret, true);
    EXPECT_TRUE(list.empty());

    list.push_back({"container-1"});
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .defaults()
        .will(returnValue(RMRS_ERROR));
    ret = GetIoBytesInfo(list);
    EXPECT_EQ(ret, false);

    GlobalMockObject::verify();
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .defaults()
        .will(returnValue(RMRS_OK));
    ret = GetIoBytesInfo(list);
    EXPECT_EQ(ret, true);
}

TEST_F(TestUcacheBottleneckDetector, GetPageCacheInfoTest)
{
    std::vector<ContainerInfo> list;
    uint32_t ret = GetPageCacheInfo(list);
    EXPECT_EQ(ret, true);
    EXPECT_TRUE(list.empty());

    list.push_back({"container-1"});
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .defaults()
        .will(returnValue(RMRS_ERROR));
    ret = GetPageCacheInfo(list);
    EXPECT_EQ(ret, false);

    GlobalMockObject::verify();
    MOCKER_CPP(&RmrsFileUtil::GetFileInfo, RMRS_RES(*)(const string &path, vector<string> &info))
        .defaults()
        .will(returnValue(RMRS_OK));
    ret = GetPageCacheInfo(list);
    EXPECT_EQ(ret, true);
}

TEST_F(TestUcacheBottleneckDetector, NoContainers)
{
    std::vector<ContainerInfo> list;
    uint32_t ret = GetCurContainerList(list);
    EXPECT_EQ(ret, RMRS_OK);
    EXPECT_TRUE(list.empty());
}

TEST_F(TestUcacheBottleneckDetector, GetCurContainerListTest)
{
    std::vector<ContainerInfo> list;
    list.push_back({"container-1"});

    MOCKER_CPP(GetIoBytesInfo, bool(*)(std::vector<ContainerInfo> &containerInfoList))
        .stubs()
        .will(returnValue(false));
    MOCKER_CPP(GetPageCacheInfo, bool(*)(std::vector<ContainerInfo> &containerInfoList))
        .stubs()
        .will(returnValue(false));

    uint32_t ret = GetCurContainerList(list);
    EXPECT_EQ(ret, RMRS_ERROR);
    EXPECT_EQ(list.size(), 1u);

    GlobalMockObject::verify();
    MOCKER_CPP(GetIoBytesInfo, bool(*)(std::vector<ContainerInfo> &containerInfoList))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(GetPageCacheInfo, bool(*)(std::vector<ContainerInfo> &containerInfoList))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(ExtractContainerCpuStat, uint32_t(*)(std::vector<ContainerInfo> &containerInfoList))
    .stubs()
    .will(returnValue(RMRS_ERROR));
    ret = GetCurContainerList(list);
    EXPECT_EQ(ret, RMRS_ERROR);

    GlobalMockObject::verify();
    MOCKER_CPP(GetIoBytesInfo, bool(*)(std::vector<ContainerInfo> &containerInfoList))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(GetPageCacheInfo, bool(*)(std::vector<ContainerInfo> &containerInfoList))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(ExtractContainerCpuStat, uint32_t(*)(std::vector<ContainerInfo> &containerInfoList))
    .stubs()
    .will(returnValue(RMRS_OK));
    ret = GetCurContainerList(list);
    EXPECT_EQ(ret, RMRS_OK);
}

}
