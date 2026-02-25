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
public:
    static constexpr int BITMAP_BUF_LEN = 10;
    static constexpr int BITMAP_BUF_LEN_PART1 = 4;
    static constexpr int BITMAP_BUF_LEN_PART2 = 6;

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
TEST_F(AccessIoctlTest, TestAccessReadAllInOneTime)
{
    int ret;
    size_t len = BITMAP_BUF_LEN;
    char buf[BITMAP_BUF_LEN];

    MOCKER(read).stubs().will(returnValue(static_cast<ssize_t>(len)));
    ret = AccessRead(len, buf);
    EXPECT_EQ(0, ret);
}

TEST_F(AccessIoctlTest, TestAccessReadPartial)
{
    int ret;
    size_t len = BITMAP_BUF_LEN;
    char buf[BITMAP_BUF_LEN];

    MOCKER(read)
        .stubs()
        .will(returnValue(static_cast<ssize_t>(len - 1)))
        .then(returnValue(static_cast<ssize_t>(0)));
    ret = AccessRead(len, buf);
    EXPECT_EQ(-EIO, ret);
}

TEST_F(AccessIoctlTest, TestAccessReadAllInTwoTimes)
{
    int ret;
    size_t len = BITMAP_BUF_LEN_PART1 + BITMAP_BUF_LEN_PART2;
    char buf[BITMAP_BUF_LEN_PART1 + BITMAP_BUF_LEN_PART2];
    constexpr int CALL_NUMS = 2;

    MOCKER(read)
        .expects(exactly(CALL_NUMS))
        .will(returnValue(static_cast<ssize_t>(BITMAP_BUF_LEN_PART1)))
        .then(returnValue(static_cast<ssize_t>(BITMAP_BUF_LEN_PART2)));
    ret = AccessRead(len, buf);
    EXPECT_EQ(0, ret);
}

TEST_F(AccessIoctlTest, TestAccessReadFailedInSecondTime)
{
    int ret;
    size_t len = BITMAP_BUF_LEN_PART1 + BITMAP_BUF_LEN_PART2;
    char buf[BITMAP_BUF_LEN_PART1 + BITMAP_BUF_LEN_PART2];
    constexpr int CALL_NUMS = 2;

    MOCKER(read)
        .expects(exactly(CALL_NUMS))
        .will(returnValue(static_cast<ssize_t>(BITMAP_BUF_LEN_PART1)))
        .then(returnValue(static_cast<ssize_t>(-EAGAIN)));
    ret = AccessRead(len, buf);
    EXPECT_EQ(-EIO, ret);
}
