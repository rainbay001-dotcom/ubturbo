/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 access ioctl ut code
 */

#include <cstdlib>
#include <sys/ioctl.h>
#include <linux/errno.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "manage/manage.h"
#include "manage/access_ioctl.h"

using namespace std;

class AccessIoctlTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

extern "C" int open(const char *pathname, int flags);
TEST_F(AccessIoctlTest, TestAccessIoctlAddPid)
{
    int ret;
    int len = 0;
    struct AccessAddPidPayload payload;
    ret = AccessIoctlAddPid(len, &payload);
    EXPECT_EQ(-EINVAL, ret);
    MOCKER((int (*)(int, unsigned long, void *))ioctl).stubs().will(returnValue(-1));
    len = 1;
    ret = AccessIoctlAddPid(len, &payload);
    EXPECT_EQ(-EBADF, ret);
}

TEST_F(AccessIoctlTest, TestAccessIoctlRemovePid)
{
    int ret;
    int len = 0;
    struct AccessRemovePidPayload payload;
    ret = AccessIoctlRemovePid(len, &payload);
    EXPECT_EQ(-EINVAL, ret);

    MOCKER((int (*)(int, unsigned long, void *))ioctl).stubs().will(returnValue(-1));
    len = 1;
    ret = AccessIoctlRemovePid(len, &payload);
    EXPECT_EQ(-EBADF, ret);
}

TEST_F(AccessIoctlTest, TestAccessIoctlRemoveAllPid)
{
    int ret;
    MOCKER(open).stubs().will(returnValue(-EPERM));
    ret = AccessIoctlRemoveAllPid();
    EXPECT_EQ(-EBADF, ret);
}

TEST_F(AccessIoctlTest, TestAccessIoctlWalkPagemap)
{
    int ret;
    size_t len;
    MOCKER(open).stubs().will(returnValue(0));
    MOCKER((int (*)(int, unsigned long, void *))ioctl).stubs().will(returnValue(-1));
    ret = AccessIoctlWalkPagemap(&len);
    EXPECT_EQ(-EBADF, ret);
}

extern "C" ssize_t read(int fd, void *buf, size_t count);
TEST_F(AccessIoctlTest, TestAccessRead)
{
    int ret;
    size_t len = 1;
    char buf[BUFFER_SIZE];
    MOCKER(open).stubs().will(returnValue(1));
    MOCKER(read).stubs().will(returnValue(0));
    MOCKER(close).stubs().will(ignoreReturnValue());
    ret = AccessRead(len, buf);
    EXPECT_EQ(-EIO, ret);
}
