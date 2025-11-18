/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef RACKMANAGER_TEST_RACK_LOG_FILTER_H
#define RACKMANAGER_TEST_RACK_LOG_FILTER_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#define private public
#include "rack_logger_filter.h"

namespace turbo::log {
class TestRackLoggerFilter : public testing::Test {
public:
    TestRackLoggerFilter() = default;

    void SetUp() override;

    void TearDown() override;

protected:
    RackLoggerFilter filter;
};
}


#endif // RACKMANAGER_TEST_RACK_LOG_FILTER_H
