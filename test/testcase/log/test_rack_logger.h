/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef RACK_MANAGER_TEST_RACK_LOGGER_ENTRY_H
#define RACK_MANAGER_TEST_RACK_LOGGER_ENTRY_H
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "securec.h"

#include "rack_logger.cpp"
#include "rack_logger_manager.h"

namespace rack::ut::log {
class TestRackLogger : public testing::Test {
public:
    TestRackLogger();
    virtual void SetUp(void);
    virtual void TearDown(void);
};
}
#endif // RACK_MANAGER_TEST_RACK_LOGGER_ENTRY_H