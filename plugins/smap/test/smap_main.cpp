/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "gtest.h"

int main(int argc, char **argv)
{
    ::testing::GTEST_FLAG(output) = "xml:gcovr_report/test_detail.xml";
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    std::cout << "Result " << ret << std::endl;
    return ret;
}
