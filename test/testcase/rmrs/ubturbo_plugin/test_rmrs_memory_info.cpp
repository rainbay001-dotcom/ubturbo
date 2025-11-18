/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.‘
 */

#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "rmrs_error.h"
#include "util/rmrs_memory_info.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
using namespace std;

namespace rmrs {

class TestRmrsMemoryInfo : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestRmrsMemoryInfo SetUp Begin]" << endl;
        cout << "[TestRmrsMemoryInfo SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestRmrsMemoryInfo TearDown Begin]" << endl;

        GlobalMockObject::verify();

        cout << "[TestRmrsMemoryInfo TearDown End]" << endl;
    }
};

// 测试空字符串输入
TEST_F(TestRmrsMemoryInfo, StringToKBEmptyInput)
{
    std::string str;
    uint64_t res = StringToKB(str);
    EXPECT_EQ(res, 0) << "Empty string should return 0";
}

// 测试无单位的数值输入
TEST_F(TestRmrsMemoryInfo, StringToKBNoUnit)
{
    std::string str = "123";
    uint64_t res = StringToKB(str);
    EXPECT_EQ(res, 0) << "String without unit should return 0";
}

// 测试有效单位的输入 - KB
TEST_F(TestRmrsMemoryInfo, StringToKBValidUnitKB)
{
    int memSize = 100;
    std::string str = "100KB";
    uint64_t res = StringToKB(str);
    EXPECT_EQ(res, memSize) << "100KB should return 100";
}

// 测试有效单位的输入 - MB
TEST_F(TestRmrsMemoryInfo, StringToKBValidUnitMB)
{
    int memSize = 50 * 1024;
    std::string str = "50MB";
    uint64_t res = StringToKB(str);
    EXPECT_EQ(res, memSize) << "50MB should return 51200";
}

// 测试有效单位的输入 - GB
TEST_F(TestRmrsMemoryInfo, StringToKBValidUnitGB)
{
    int memSize = 2 * 1024 * 1024;
    std::string str = "2GB";
    uint64_t res = StringToKB(str);
    EXPECT_EQ(res, memSize) << "2GB should return 2097152";
}

// 测试有效单位的输入 - TB
TEST_F(TestRmrsMemoryInfo, StringToKBValidUnitTB)
{
    int memSize = 1024 * 1024 * 1024;
    std::string str = "1TB";
    uint64_t res = StringToKB(str);
    EXPECT_EQ(res, memSize) << "1TB should return 1073741824";
}

// 测试无效单位的输入
TEST_F(TestRmrsMemoryInfo, StringToKBInvalidUnit)
{
    std::string str = "100XX";
    uint64_t res = StringToKB(str);
    EXPECT_EQ(res, 0) << "Invalid unit should return 0";
}

// 测试数值包含小数点的情况
TEST_F(TestRmrsMemoryInfo, StringToKBWithDecimal)
{
    int result = 12;
    std::string str = "12.5KB";
    uint64_t res = StringToKB(str);
    EXPECT_EQ(res, result) << "12.5KB should return 12";
}

// 测试正常情况：所有字段都存在且值有效
TEST_F(TestRmrsMemoryInfo, ParseNodeMemoryInfoMap_Success)
{
    // 构造一个有效的JSON字符串
    string jsonStr = R"({
        "timestamp": "1625097600",
        "nodeId": "node001",
        "totalMemory": "8192MB",
        "usedMemory": "4096MB",
        "freeMemory": "4096MB",
        "borrowedMemory": "0MB",
        "lentMemory": "0MB",
        "numaMemInfo": {},
        "borrowedAndLentInfo": {}
    })";

    NodeMemoryInfo nodeMemoryInfoList;
    JSON_VEC nodeMemoryInfoListVec;
    nodeMemoryInfoListVec.push_back(jsonStr);

    JSON_MAP nodeMemoryInfoMap;
    nodeMemoryInfoMap["timestamp"] = "1625097600";
    nodeMemoryInfoMap["totalMemory"] = "8192MB";
    nodeMemoryInfoMap["usedMemory"] = "4096MB";
    nodeMemoryInfoMap["freeMemory"] = "4096MB";
    nodeMemoryInfoMap["borrowedMemory"] = "0MB";
    nodeMemoryInfoMap["lentMemory"] = "0MB";

    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    obj.ParseNodeMemoryInfoMap(nodeMemoryInfoListVec, 0, nodeMemoryInfoMap);

    int result1 = 8192 * 1024;
    int result2 = 4096 * 1024;
    int result3 = 1625097600;
    // 验证nodeMemoryInfoList中的值
    EXPECT_EQ(static_cast<time_t>(result3), obj.nodeMemoryInfoList[0].timestamp);
    EXPECT_EQ("node001", obj.nodeMemoryInfoList[0].nodeId);
    EXPECT_EQ(static_cast<uint64_t>(result1), obj.nodeMemoryInfoList[0].totalMemory);
    EXPECT_EQ(static_cast<uint64_t>(result2), obj.nodeMemoryInfoList[0].usedMemory);
    EXPECT_EQ(static_cast<uint64_t>(result2), obj.nodeMemoryInfoList[0].freeMemory);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].borrowedMemory);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].lentMemory);
}

TEST_F(TestRmrsMemoryInfo, ParseNodeMemoryInfoMap_Failed)
{
    // 构造一个有效的JSON字符串
    string jsonStr = R"({
        "timestamp": "1625097600",
        "nodeId": "node001",
        "totalMemory": "8192MB",
        "usedMemory": "4096MB",
        "freeMemory": "4096MB",
        "borrowedMemory": "0MB",
        "lentMemory": "0MB",
        "numaMemInfo": {},
        "borrowedAndLentInfo": {}
    })";

    NodeMemoryInfo nodeMemoryInfoList;
    JSON_VEC nodeMemoryInfoListVec;
    nodeMemoryInfoListVec.push_back(jsonStr);

    JSON_MAP nodeMemoryInfoMap;
    nodeMemoryInfoMap["timestamp"] = "1625097600";
    nodeMemoryInfoMap["totalMemory"] = "8192MB";
    nodeMemoryInfoMap["usedMemory"] = "4096MB";
    nodeMemoryInfoMap["freeMemory"] = "4096MB";
    nodeMemoryInfoMap["borrowedMemory"] = "0MB";
    nodeMemoryInfoMap["lentMemory"] = "0MB";

    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(false));
    bool res = obj.ParseNodeMemoryInfoMap(nodeMemoryInfoListVec, 0, nodeMemoryInfoMap);
    EXPECT_EQ(res, 1);
}

TEST_F(TestRmrsMemoryInfo, ParseNumaMemInfoMap_Success)
{
    JSON_VEC numaMemInfoVec;
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(true));
    JSON_MAP numaMemInfoMap;
    numaMemInfoMap["numaId"] = "1";
    numaMemInfoMap["socketId"] = "0";
    numaMemInfoMap["memTotal"] = "8192MB";
    numaMemInfoMap["memFree"] = "4096MB";
    numaMemInfoMap["memUsed"] = "4096MB";
    numaMemInfoMap["vmMemTotal"] = "0MB";
    numaMemInfoMap["vmMemFree"] = "0MB";
    numaMemInfoMap["vmMemUsed"] = "4096MB";
    numaMemInfoMap["reservedMem"] = "4096MB";
    numaMemInfoMap["lentMem"] = "0MB";
    numaMemInfoMap["sharedMem"] = "0MB";

    NodeMemoryInfoList obj;
    NodeMemoryInfo nodeMemoryInfo;
    RackNumaMemInfo numaMemInfo = {};
    nodeMemoryInfo.numaMemInfo.push_back(numaMemInfo);
    obj.nodeMemoryInfoList.push_back(nodeMemoryInfo);
    obj.ParseNumaMemInfoMap(numaMemInfoVec, 0, 0, numaMemInfoMap);
    int result1 = 8192 * 1024;
    int result2 = 4096 * 1024;
    // 验证numaMemInfo中的值
    EXPECT_EQ(static_cast<uint16_t>(1), obj.nodeMemoryInfoList[0].numaMemInfo[0].numaId);
    EXPECT_EQ(static_cast<uint64_t>(result1), obj.nodeMemoryInfoList[0].numaMemInfo[0].memTotal);
    EXPECT_EQ(static_cast<uint64_t>(result2), obj.nodeMemoryInfoList[0].numaMemInfo[0].memFree);
    EXPECT_EQ(static_cast<uint64_t>(result2), obj.nodeMemoryInfoList[0].numaMemInfo[0].memUsed);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].numaMemInfo[0].vmMemTotal);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].numaMemInfo[0].vmMemFree);
    EXPECT_EQ(static_cast<uint64_t>(result2), obj.nodeMemoryInfoList[0].numaMemInfo[0].vmUsedMem);
    EXPECT_EQ(static_cast<uint64_t>(result2), obj.nodeMemoryInfoList[0].numaMemInfo[0].reservedMem);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].numaMemInfo[0].lentMem);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].numaMemInfo[0].sharedMem);
}

TEST_F(TestRmrsMemoryInfo, ParseNumaMemInfoMap_Failed)
{
    JSON_VEC numaMemInfoVec;
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(false));
    JSON_MAP numaMemInfoMap;
    numaMemInfoMap["numaId"] = "1";
    numaMemInfoMap["socketId"] = "0";
    numaMemInfoMap["memTotal"] = "8192MB";
    numaMemInfoMap["memFree"] = "4096MB";
    numaMemInfoMap["memUsed"] = "4096MB";
    numaMemInfoMap["vmMemTotal"] = "0MB";
    numaMemInfoMap["vmMemFree"] = "0MB";
    numaMemInfoMap["vmUsedMem"] = "4096MB";
    numaMemInfoMap["reservedMem"] = "4096MB";
    numaMemInfoMap["lentMem"] = "0MB";
    numaMemInfoMap["sharedMem"] = "0MB";

    NodeMemoryInfoList obj;
    NodeMemoryInfo nodeMemoryInfo;
    RackNumaMemInfo numaMemInfo = {};
    nodeMemoryInfo.numaMemInfo.push_back(numaMemInfo);
    obj.nodeMemoryInfoList.push_back(nodeMemoryInfo);
    bool res = obj.ParseNumaMemInfoMap(numaMemInfoVec, 0, 0, numaMemInfoMap);
    EXPECT_EQ(res, 1);
}

TEST_F(TestRmrsMemoryInfo, ParseBorrowedItemMap_Success)
{
    JSON_VEC borrowedItemVec;
    JSON_MAP borrowedItemMap;
    borrowedItemMap["nodeId"] = "1";
    borrowedItemMap["numaId"] = "2";
    borrowedItemMap["memory"] = "4096MB";
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(true));
    NodeMemoryInfoList obj;
    NodeMemoryInfo nodeMemoryInfo;
    RackMemNumaPair rackMemNumaPair = {};
    std::vector<RackMemNumaPair> borrowedItem;
    RackBorrowedAndLentInfo rackBorrowedAndLentInfo;
    rackBorrowedAndLentInfo.borrowedItem.push_back(rackMemNumaPair);
    nodeMemoryInfo.borrowedAndLentInfo = rackBorrowedAndLentInfo;
    obj.nodeMemoryInfoList.push_back(nodeMemoryInfo);
    auto res = obj.ParseBorrowedItemMap(borrowedItemVec, 0, 0, borrowedItemMap);
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMemoryInfo, ParseLentItemMap_Success)
{
    JSON_VEC lentItemVec;
    JSON_MAP lentItemMap;
    lentItemMap["nodeId"] = "1";
    lentItemMap["numaId"] = "2";
    lentItemMap["memory"] = "4096MB";
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(true));
    NodeMemoryInfoList obj;
    NodeMemoryInfo nodeMemoryInfo;
    RackMemNumaPair rackMemNumaPair = {};
    std::vector<RackMemNumaPair> borrowedItem;
    RackBorrowedAndLentInfo rackBorrowedAndLentInfo;
    rackBorrowedAndLentInfo.lentItem.push_back(rackMemNumaPair);
    nodeMemoryInfo.borrowedAndLentInfo = rackBorrowedAndLentInfo;
    obj.nodeMemoryInfoList.push_back(nodeMemoryInfo);
    auto res = obj.ParseLentItemMap(lentItemVec, 0, 0, lentItemMap);
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMemoryInfo, CreateNodeMemoryInfoListVec_Success)
{
    std::string jsonString;
    JSON_MAP nodeMemoryInfoListMAP;
    JSON_VEC nodeMemoryInfoListVec;

    nodeMemoryInfoListMAP["nodeMemoryInfoList"] = "1";

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(returnValue(true));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateNodeMemoryInfoListVec(jsonString, nodeMemoryInfoListMAP, nodeMemoryInfoListVec);
    ASSERT_EQ(res, 0);
}

TEST_F(TestRmrsMemoryInfo, CreateNodeMemoryInfoListVec_Failed1)
{
    std::string jsonString;
    JSON_MAP nodeMemoryInfoListMAP;
    JSON_VEC nodeMemoryInfoListVec;

    nodeMemoryInfoListMAP["nodeMemoryInfoList"] = "1";

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(false));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateNodeMemoryInfoListVec(jsonString, nodeMemoryInfoListMAP, nodeMemoryInfoListVec);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMemoryInfo, CreateNodeMemoryInfoListVec_Failed2)
{
    std::string jsonString;
    JSON_MAP nodeMemoryInfoListMAP;
    JSON_VEC nodeMemoryInfoListVec;

    nodeMemoryInfoListMAP["nodeMemoryInfoList"] = "1";

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(returnValue(false));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateNodeMemoryInfoListVec(jsonString, nodeMemoryInfoListMAP, nodeMemoryInfoListVec);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMemoryInfo, CreateNumaMemInfoVec_Success)
{
    JSON_VEC numaMemInfoVec;
    JSON_MAP nodeMemoryInfoMap;
    numaMemInfoVec.push_back("1");
    nodeMemoryInfoMap["numaMemInfo"] = "1";
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(returnValue(true));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateNumaMemInfoVec(numaMemInfoVec, 0, nodeMemoryInfoMap);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMemoryInfo, CreateBorrowedItemVec_Failed)
{
    JSON_VEC borrowedItemVec;
    JSON_MAP borrowedAndLentInfoMap;
    borrowedAndLentInfoMap["borrowedItem"] = "1";
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(returnValue(false));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateBorrowedItemVec(borrowedItemVec, 0, borrowedAndLentInfoMap);
    ASSERT_EQ(res, 1);
}

TEST_F(TestRmrsMemoryInfo, CreateBorrowedItemVec_Success1)
{
    JSON_VEC borrowedItemVec;
    JSON_MAP borrowedAndLentInfoMap;
    borrowedItemVec.push_back("1");
    borrowedAndLentInfoMap["borrowedItem"] = "1";
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(returnValue(true));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateBorrowedItemVec(borrowedItemVec, 0, borrowedAndLentInfoMap);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMemoryInfo, CreateLentItemVec_Success1)
{
    JSON_VEC lentItemVec;
    JSON_MAP borrowedAndLentInfoMap;
    lentItemVec.push_back("1");
    borrowedAndLentInfoMap["lentItem"] = "1";
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(returnValue(true));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateLentItemVec(lentItemVec, 0, borrowedAndLentInfoMap);
    ASSERT_EQ(res, true);
}

TEST_F(TestRmrsMemoryInfo, FromJson_Failed1)
{
    std::string jsonString;
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    MOCKER_CPP(
        &rmrs::NodeMemoryInfoList::CreateNodeMemoryInfoListVec,
        bool (*)(const std::string &jsonString, JSON_MAP &nodeMemoryInfoListMAP, JSON_VEC &nodeMemoryInfoListVec))
        .stubs()
        .will(returnValue(true));
    obj.FromJson(jsonString);
}

TEST_F(TestRmrsMemoryInfo, FromJson_Succeed1)
{
    std::string jsonString = R"({
    "nodeMemoryInfoList": [
        {
            "timestamp": "1744025731",
            "nodeId": "Node0",
            "totalMemory": "942.2GB",
            "usedMemory": "69.6GB",
            "freeMemory": "872.6GB",
            "borrowedMemory": "4.0GB",
            "lentMemory": "0.0GB",
            "numaMemInfo": [
                {
                    "numaId": 0,
                    "socketId": 0,
                    "memTotal": "251.7GB",
                    "memFree": "243.2GB",
                    "memUsed": "8.6GB",
                    "memUsageRate": "3.4%",
                    "vmMemTotal": "0MB",
                    "vmMemFree": "0MB",
                    "vmMemUsed": "0MB",
                    "vmMemUsageRate": "0.0%",
                    "reservedMem": "62.9GB",
                    "lentMem": "0.0GB",
                    "sharedMem": "0.0GB"
                }
            ],
            "borrowedAndLentInfo": {
                "borrowedItem": [
                    {
                        "nodeId": "Node1",
                        "numaId": 2,
                        "memory": "4.1GB"
                    }
                ],
                "lentItem": [
                ]
            }
        }
    ]
})";
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    obj.FromJson(jsonString);
}

TEST_F(TestRmrsMemoryInfo, ToString_Succeed1)
{
    int memSize = 1024;
    NodeMemoryInfoWithReservedMem obj;
    obj.reservedMem = memSize;
    obj.timestamp = std::time(nullptr);
    obj.ToString();
}

} // namespace rmrs