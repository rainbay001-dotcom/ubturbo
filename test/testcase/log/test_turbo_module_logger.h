/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef TEST_TURBO_MODULE_LOGGER_H
#define TEST_TURBO_MODULE_LOGGER_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "turbo_module_logger.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace turbo::log {
class TestTurboModuleLogger : public testing::Test {
public:
    TestTurboModuleLogger() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace turbo::log
#endif // TEST_TURBO_MODULE_LOGGER_H