/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.‘
 */

#include <gmock/gmock.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "rmrs_json_util.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;
namespace rmrs {

class TestRmrsJsonUtil : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestRmrsJsonUtil SetUp Begin]" << endl;
        cout << "[TestRmrsJsonUtil SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestRmrsJsonUtil TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestRmrsJsonUtil TearDown End]" << endl;
    }
};

TEST_F(TestRmrsJsonUtil, RackMemConvertMap2JsonStrSucceed)
{
    JSON_MAP strMap1;
    JSON_STR jsonStr1;
    strMap1["a"] = "b";
    bool res = JsonUtil::RackMemConvertMap2JsonStr(strMap1, jsonStr1);
    EXPECT_EQ(res, true);
}

TEST_F(TestRmrsJsonUtil, RackMemConvertVector2JsonStrSucceed)
{
    JSON_VEC vec1;
    JSON_STR jsonStr1;
    vec1.push_back("a");
    bool res = JsonUtil::RackMemConvertVector2JsonStr(vec1, jsonStr1);
    EXPECT_EQ(res, true);
}

TEST_F(TestRmrsJsonUtil, RackMemConvertJsonStr2VecSucceed)
{
    JSON_VEC vec1;
    JSON_VEC vec2;
    JSON_STR jsonStr1;
    vec1.push_back("a");
    bool res = JsonUtil::RackMemConvertVector2JsonStr(vec1, jsonStr1);
    EXPECT_EQ(res, true);
    res = JsonUtil::RackMemConvertJsonStr2Vec(jsonStr1, vec2);
    EXPECT_EQ(res, true);
}

TEST_F(TestRmrsJsonUtil, RackMemConvertJsonStr2MapSucceed)
{
    JSON_MAP map1;
    JSON_STR jsonStr1;
    map1["a"] = "b";
    bool res = JsonUtil::RackMemConvertMap2JsonStr(map1, jsonStr1);
    EXPECT_EQ(res, true);
    res = JsonUtil::RackMemConvertJsonStr2Map(jsonStr1, map1);
    EXPECT_EQ(res, true);
}

}