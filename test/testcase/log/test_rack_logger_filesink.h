/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef RACKMANAGER_TEST_RACK_LOG_FILESINK_H
#define RACKMANAGER_TEST_RACK_LOG_FILESINK_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#define private public
#include "rack_logger_filesink.h"

namespace turbo::log {
const std::string FILE_PATH = "/log/run/";
const std::string FILE_NAME = "test_log";
class TestRackLoggerFilesink : public testing::Test {
public:
    TestRackLoggerFilesink() = default;

    void SetUp() override;

    void TearDown() override;
};
}


#endif // RACKMANAGER_TEST_RACK_LOG_FILESINK_H
