/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "rmrs_json_helper.h"
#include "rmrs_json_util.h"
#include "rmrs_serialize.h"
#include "rmrs_serializer.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace rmrs::serialization {
using namespace rmrs;
using rmrs::serialize::RmrsOutStream;
using rmrs::serialize::RmrsInStream;

// 测试类
class TestRmrsSerializer : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestRmrsSerializer, PidNumaInfoCollectParam_Serialize_Succeed)
{
    std::vector<pid_t> pidList{1, 2};

    PidNumaInfoCollectParam param1;
    param1.pidList = pidList;
    RmrsOutStream outBuilder;
    outBuilder << param1;
    RmrsInStream inBuilder(outBuilder.GetBufferPointer(), outBuilder.GetSize());
    PidNumaInfoCollectParam param2;
    inBuilder >> param2;
 
    EXPECT_EQ(param1.pidList[0], param2.pidList[0]);
    EXPECT_EQ(param1.pidList[1], param2.pidList[1]);
}
 
TEST_F(TestRmrsSerializer, PidNumaInfoCollectResult_Serialize_Succeed)
{
    std::vector<mempooling::PidInfo> pidInfoList = {
        {1, {0}, 1024, 2048, {{0, 1024, true, 0}, {1, 2048, false, -1}}},
        {2, {1, 2}, 4096, 8192, {{1, 4096, true, 0}, {2, 8192, true, 1}}},
    };
 
    PidNumaInfoCollectResult param1;
    param1.pidInfoList = pidInfoList;
    RmrsOutStream outBuilder;
    outBuilder << param1;
    PidNumaInfoCollectResult param2;
    RmrsInStream inBuilder(outBuilder.GetBufferPointer(), outBuilder.GetSize());
    inBuilder >> param2;

    ASSERT_EQ(param1.pidInfoList.size(), param2.pidInfoList.size());

    for (size_t i = 0; i < param1.pidInfoList.size(); ++i) {
        const auto &a = param1.pidInfoList[i];
        const auto &b = param2.pidInfoList[i];

        EXPECT_EQ(a.pid, b.pid);
        EXPECT_EQ(a.localNumaIds, b.localNumaIds);
        EXPECT_EQ(a.totalLocalUsedMem, b.totalLocalUsedMem);
        EXPECT_EQ(a.totalRemoteUsedMem, b.totalRemoteUsedMem);

        ASSERT_EQ(a.metaNumaInfos.size(), b.metaNumaInfos.size());
        for (size_t j = 0; j < a.metaNumaInfos.size(); ++j) {
            EXPECT_EQ(a.metaNumaInfos[j].numaId, b.metaNumaInfos[j].numaId);
            EXPECT_EQ(a.metaNumaInfos[j].numaUsedMem, b.metaNumaInfos[j].numaUsedMem);
            EXPECT_EQ(a.metaNumaInfos[j].isLocalNuma, b.metaNumaInfos[j].isLocalNuma);
            EXPECT_EQ(a.metaNumaInfos[j].socketId, b.metaNumaInfos[j].socketId);
        }
    }
}

} // namespace rmrs::serialization