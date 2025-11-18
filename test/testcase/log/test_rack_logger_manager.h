/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef RACK_MANAGER_TEST_RACK_LOGGER_MANAGER_H
#define RACK_MANAGER_TEST_RACK_LOGGER_MANAGER_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#define private public
#include "rack_logger_manager.h"


#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace turbo::log {
const std::string FILE_PATH = "/log/";
class TestRackLoggerManager : public testing::Test {
public:
    TestRackLoggerManager() = default;

    void SetUp() override;

    void TearDown() override;

private:
    std::string currentPath;
};
}
#endif // RACK_MANAGER_TEST_RACK_LOGGER_MANAGER_H