/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "rmrs_time_util.h"

using namespace std;

namespace rmrs {
class TestRmrsTimeUtil : public ::testing::Test {
    void SetUp() override
    {
        cout << "[TestRmrsTimeUtil SetUp Begin]" << endl;
        cout << "[TestRmrsTimeUtil SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestRmrsTimeUtil TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestRmrsTimeUtil TearDown End]" << endl;
    }
};

TEST_F(TestRmrsTimeUtil, CharPtrToTime_Success)
{
    const char *validTimeStr = "2023-10-13 14:30:00";
    time_t resultTime;

    RMRS_RES res = RmrsTimeUtil::CharPtrToTime(validTimeStr, resultTime, DateFormatMode::SPACE_SEPARATOR_MODE);
    ASSERT_EQ(res, RMRS_OK);

    int intervalYear = 1900;
    int realYear = 2023;
    int realMonth = 10;
    int realDay = 13;
    int realHour = 14;
    int realMin = 30;

    // 用 localtime 转换 resultTime 检查其是否匹配
    struct tm *tmResult = localtime(&resultTime);
    ASSERT_EQ(tmResult->tm_year + intervalYear, realYear); // 检查年份
    ASSERT_EQ(tmResult->tm_mon + 1, realMonth);       // 检查月份
    ASSERT_EQ(tmResult->tm_mday, realDay);          // 检查日期
    ASSERT_EQ(tmResult->tm_hour, realHour);          // 检查小时
    ASSERT_EQ(tmResult->tm_min, realMin);           // 检查分钟
    ASSERT_EQ(tmResult->tm_sec, 0);            // 检查秒钟
}

TEST_F(TestRmrsTimeUtil, GetCurrentTimeT_Success)
{
    // 获取当前的系统时间
    time_t currentTime = time(nullptr);

    // 调用 GetCurrentTimeT() 获取返回的时间
    time_t resultTime = RmrsTimeUtil::GetCurrentTimeT();

    // 将当前时间和返回的时间做比较
    // 由于时间可能略有偏差（例如测试函数调用时的延迟），我们允许一定范围内的误差
    ASSERT_GE(resultTime, currentTime - 1);  // 返回时间不能比当前时间少 1 秒
    ASSERT_LE(resultTime, currentTime + 1);  // 返回时间不能比当前时间多 1 秒
}

TEST_F(TestRmrsTimeUtil, GetOneHourAgoTimeT_Success)
{
     // 获取当前时间戳
    time_t currentTime = RmrsTimeUtil::GetCurrentTimeT();
    
    // 计算预期时间，即当前时间减去 1 小时（3600 秒），然后转换成小时数
    time_t expectedTime = currentTime - 3600;  // 1 小时 = 3600 秒
    auto expectedHours = expectedTime / 3600;  // 转换为小时数

    // 获取函数返回的时间戳，并转换为小时数
    time_t resultTime = RmrsTimeUtil::GetOneHourAgoTimeT();
    auto resultHours = resultTime;

    // 比较小时数是否相等
    ASSERT_EQ(resultHours, expectedHours);
}
}