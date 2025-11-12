/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user thread ut code
 */

#include <cstdlib>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "manage/manage.h"
#include "manage/thread.h"

using namespace std;


class ThreadTest : public ::testing::Test {
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

extern "C" void EnvMutexLock(EnvMutex *mutex);
extern "C" void EnvMutexUnlock(EnvMutex *mutex);
extern "C" void *ThreadMain(void *args);
extern "C" int IdleWork(struct ThreadContext *priv);
int TmpWorkFunc(ThreadCtx *priv)
{
    return 0;
}

TEST_F(ThreadTest, TestThreadMainStoped)
{
    ThreadCtx *ctx = (ThreadCtx *)malloc(sizeof(*ctx));
    ctx->workFunc = IdleWork;
    EnvAtomicSet(&ctx->stop, 1);
    ctx->period = 50;

    void *ret = ThreadMain(ctx);
    EXPECT_EQ(nullptr, ret);
}

TEST_F(ThreadTest, TestInitThread)
{
    struct ProcessManager pm = {
        .nrThread = 0
    };
    uint32_t period;
    int ret;

    MOCKER(pthread_create).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    ret = InitThread(&pm, period, TmpWorkFunc);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, pm.nrThread);
    free(pm.threadCtx[0]);
}

TEST_F(ThreadTest, TestDestroyAllThread)
{
    int ret;
    struct ProcessManager pm = { .nrThread = 2 };
    pm.threadCtx[0] = malloc(sizeof(ThreadCtx));
    pm.threadCtx[1] = malloc(sizeof(ThreadCtx));
    ASSERT_NE(nullptr, pm.threadCtx[0]);
    ASSERT_NE(nullptr, pm.threadCtx[1]);
    MOCKER(pthread_join).expects(exactly(2)).will(ignoreReturnValue());
    MOCKER(EnvMutexLock).stubs().will(ignoreReturnValue());
    MOCKER(EnvMutexUnlock).stubs().will(ignoreReturnValue());
    ret = DestroyAllThread(&pm);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(nullptr, pm.threadCtx[0]);
    EXPECT_EQ(nullptr, pm.threadCtx[1]);
    EXPECT_EQ(0, pm.nrThread);
}