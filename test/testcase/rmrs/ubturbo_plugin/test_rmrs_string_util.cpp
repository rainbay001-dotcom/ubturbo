/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */

#include <gmock/gmock.h>
#include <stdexcept>
#include <limits>
#include <vector>
#include <string>

#include "util/rmrs_string_util.h"

namespace rmrs {

class TestRmrsStringUtil : public ::testing::Test {
protected:
    TestRmrsStringUtil() {}

    ~TestRmrsStringUtil() override {}
};

// 测试 SafeStopid
TEST_F(TestRmrsStringUtil, SafeStopid_ValidInput)
{
    pid_t result = 123;
    EXPECT_EQ(RmrsStringUtil::SafeStopid("123"), result);
}

TEST_F(TestRmrsStringUtil, SafeStopid_InValidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStopid(""), 0);
    // RmrsStringUtil::SafeStopid("");
}

TEST_F(TestRmrsStringUtil, SafeStopid_InvalidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStopid("abc"), 0);
}

TEST_F(TestRmrsStringUtil, SafeStopid_OutOfRange)
{
    EXPECT_EQ(RmrsStringUtil::SafeStopid(std::to_string(std::numeric_limits<pid_t>::max() + 1)), 0);
}

// 测试 SafeStoul
TEST_F(TestRmrsStringUtil, SafeStoul_ValidInput)
{
    uint32_t result = 123;
    EXPECT_EQ(RmrsStringUtil::SafeStoul("123"), result);
}

TEST_F(TestRmrsStringUtil, SafeStoul_InValidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoul(""), 0);
}

TEST_F(TestRmrsStringUtil, SafeStoul_InvalidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoul("abc"), 0);
}

// 测试 SafeStoull
TEST_F(TestRmrsStringUtil, SafeStoull_ValidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoull("1234567890"), 1234567890ULL);
}

TEST_F(TestRmrsStringUtil, SafeStoull_InvalidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoull("abc"), 0);
}

// 测试 SafeStoi16
TEST_F(TestRmrsStringUtil, SafeStoi16_ValidInput)
{
    int result = 123;
    EXPECT_EQ(RmrsStringUtil::SafeStoi16("123"), result);
}

TEST_F(TestRmrsStringUtil, SafeStoi16_InvalidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoi16("abc"), 0);
}

TEST_F(TestRmrsStringUtil, SafeStoi16_OutOfRange)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoi16(std::to_string(std::numeric_limits<int16_t>::max() + 1)), 0);
    EXPECT_EQ(RmrsStringUtil::SafeStoi16(std::to_string(std::numeric_limits<int16_t>::min() - 1)), 0);
}

// 测试 SafeStou16
TEST_F(TestRmrsStringUtil, SafeStou16_ValidInput)
{
    std::uint16_t result = 12345;
    EXPECT_EQ(RmrsStringUtil::SafeStou16("12345"), result);
}

TEST_F(TestRmrsStringUtil, SafeStou16_InValidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStou16(""), 0);
}

TEST_F(TestRmrsStringUtil, SafeStou16_InvalidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStou16("abc"), 0);
}

TEST_F(TestRmrsStringUtil, SafeStou16_OutOfRange)
{
    EXPECT_EQ(RmrsStringUtil::SafeStou16(std::to_string(std::numeric_limits<uint16_t>::max() + 1)), 0);
}

// 测试 SafeStoi64
TEST_F(TestRmrsStringUtil, SafeStoi64_ValidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoi64("12345678901234"), 12345678901234LL);
}

TEST_F(TestRmrsStringUtil, SafeStoi64_InValidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoi64(""), 0);
}

TEST_F(TestRmrsStringUtil, SafeStoi64_InvalidInput)
{
    EXPECT_EQ(RmrsStringUtil::SafeStoi64("abc"), 0);
}

// 测试 SafeStof
TEST_F(TestRmrsStringUtil, SafeStof_ValidInput)
{
    float_t result;
    EXPECT_EQ(RmrsStringUtil::SafeStof("123.45", result), RMRS_OK);
    EXPECT_EQ(result, 123.45f);
}

TEST_F(TestRmrsStringUtil, SafeStof_InValidInput)
{
    float_t result;
    EXPECT_EQ(RmrsStringUtil::SafeStof("", result), RMRS_ERROR);
}

TEST_F(TestRmrsStringUtil, SafeStof_InvalidInput)
{
    float_t result;
    EXPECT_EQ(RmrsStringUtil::SafeStof("abc", result), RMRS_ERROR_INVAL);
}

TEST_F(TestRmrsStringUtil, SafeStof_OutOfRange)
{
    float_t result;
    EXPECT_EQ(RmrsStringUtil::SafeStof("1e1000", result), RMRS_ERROR_EXCEEDS_RANGE);  // Exceeds range for float
}

}  // namespace rmrs