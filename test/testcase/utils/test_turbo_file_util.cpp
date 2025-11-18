/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.h>
#include <mockcpp/mokc.h>
#include <securec.h>
#include <unistd.h>
#include "turbo_file_util.h"
extern "C" {
ssize_t readlink(const char *path, char *buf, size_t bufsize);
}
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace turbo::utils {
using namespace turbo::common;
class TestTurboFileUtil : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestTurboFileUtil, GetExecutablePathShouldReturnEmptyWhenCountIsZero)
{
    MOCKER(readlink).stubs().will(returnValue(0));
    EXPECT_EQ(TurboFileUtil::GetExecutablePath(), std::string{});

    GlobalMockObject::verify();
    MOCKER(readlink).stubs().will(returnValue(MAX_PATH_LEN));
    EXPECT_EQ(TurboFileUtil::GetExecutablePath(), std::string{});
}

TEST_F(TestTurboFileUtil, GetExecutablePathShouldReturnStr)
{
    char *filePath = "/proc/self/test";
    MOCKER(readlink)
        .expects(once())
        .with(any(), outBoundP(filePath, strlen(filePath)), any())
        .will(returnValue(strlen(filePath)));
    EXPECT_EQ(TurboFileUtil::GetExecutablePath(), "/proc/self/test");
}

TEST_F(TestTurboFileUtil, GetExecutableRootDirShouldReturnProc)
{
    char *filePath = "/proc/self/test";
    MOCKER(readlink)
        .expects(once())
        .with(any(), outBoundP(filePath, strlen(filePath)), any())
        .will(returnValue(strlen(filePath)));
    EXPECT_EQ(TurboFileUtil::GetExecutableRootDir(), "/proc");
}
} // namespace turbo::utils