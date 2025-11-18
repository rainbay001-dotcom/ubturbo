/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include "turbo_type.h"

namespace turbo::utils {
class TestTurboType : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestTurboType, ConvertShouldReturnTrueWhenInputIsTrue)
{
    std::string input = "true";
    bool result = Convert<bool>(input);
    EXPECT_TRUE(result);
}

TEST_F(TestTurboType, ConvertShouldReturnFalseWhenInputIsFalse)
{
    std::string input = "false";
    bool result = Convert<bool>(input);
    EXPECT_FALSE(result);
}

TEST_F(TestTurboType, ConvertShouldThrowInvalidArgumentWhenInputIsInvalidBoolean)
{
    std::string input = "invalid";
    EXPECT_THROW(Convert<bool>(input), std::invalid_argument);
}

TEST_F(TestTurboType, ConvertShouldReturnUnsignedIntWhenInputIsUnsignedInt)
{
    std::string input = "12345";
    auto result = Convert<unsigned int>(input);
    EXPECT_EQ(result, 12345);
}

TEST_F(TestTurboType, ConvertShouldThrowInvalidArgumentWhenInputIsInvalidUnsignedInt)
{
    std::string input = "invalid";
    EXPECT_THROW(Convert<unsigned int>(input), std::invalid_argument);
}

TEST_F(TestTurboType, ConvertShouldThrowInvalidArgumentWhenInputIsUnsignedIntExceedsLimit)
{
    std::string input = "18446744073709551616"; // 2^64
    EXPECT_THROW(Convert<unsigned int>(input), std::invalid_argument);
}

TEST_F(TestTurboType, ConvertShouldReturnFloatWhenInputIsFloat)
{
    std::string input = "123.456";
    auto result = Convert<float>(input);
    EXPECT_FLOAT_EQ(result, 123.456);
}

TEST_F(TestTurboType, ConvertShouldThrowInvalidArgumentWhenInputIsInvalidFloat)
{
    std::string input = "invalid";
    EXPECT_THROW(Convert<float>(input), std::invalid_argument);
}

TEST_F(TestTurboType, ConvertShouldReturnStringWhenInputIsString)
{
    std::string input = "hello";
    auto result = Convert<std::string>(input);
    EXPECT_EQ(result, "hello");
}

} // namespace turbo::utils
