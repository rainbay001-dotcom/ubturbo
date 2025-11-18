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


#include "rmrs_json_util.h"
#include "rmrs_json_helper.h"
#include "rmrs_serializer.h"
#include "rmrs_serialize.h"

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
        {1, 1024, {0}, 0, 1, 0},
        {2, 2048, {0}, 0, 1, 1},
    };
 
    PidNumaInfoCollectResult param1;
    param1.pidInfoList = pidInfoList;
    RmrsOutStream outBuilder;
    outBuilder << param1;
    PidNumaInfoCollectResult param2;
    RmrsInStream inBuilder(outBuilder.GetBufferPointer(), outBuilder.GetSize());
    inBuilder >> param2;
    EXPECT_EQ(param1.pidInfoList[0].pid, param2.pidInfoList[0].pid);
    EXPECT_EQ(param1.pidInfoList[0].localUsedMem, param2.pidInfoList[0].localUsedMem);
    EXPECT_EQ(param1.pidInfoList[0].localNumaIds, param2.pidInfoList[0].localNumaIds);
    EXPECT_EQ(param1.pidInfoList[0].remoteUsedMem, param2.pidInfoList[0].remoteUsedMem);
    EXPECT_EQ(param1.pidInfoList[0].remoteNumaId, param2.pidInfoList[0].remoteNumaId);
    EXPECT_EQ(param1.pidInfoList[0].socketId, param2.pidInfoList[0].socketId);
    EXPECT_EQ(param1.pidInfoList[1].pid, param2.pidInfoList[1].pid);
    EXPECT_EQ(param1.pidInfoList[1].localUsedMem, param2.pidInfoList[1].localUsedMem);
    EXPECT_EQ(param1.pidInfoList[1].localNumaIds, param2.pidInfoList[1].localNumaIds);
    EXPECT_EQ(param1.pidInfoList[1].remoteUsedMem, param2.pidInfoList[1].remoteUsedMem);
    EXPECT_EQ(param1.pidInfoList[1].remoteNumaId, param2.pidInfoList[1].remoteNumaId);
    EXPECT_EQ(param1.pidInfoList[1].socketId, param2.pidInfoList[1].socketId);
}

} // namespace rmrs::serialization