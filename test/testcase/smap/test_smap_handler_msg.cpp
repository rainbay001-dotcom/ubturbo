/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.h>
#include <mockcpp/mokc.h>
#include <cerrno>
#include "smap_handler_msg.h"
#include "securec.h"

constexpr int NUM_2 = 2;
constexpr int NUM_3 = 3;
constexpr int NUM_4 = 4;
constexpr int NUM_5 = 5;
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace turbo::smap::codec;
class TestSmapHandlerMsg : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

std::atomic<bool> g_mock_new_should_fail(false);

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    if (g_mock_new_should_fail) {
        std::cout << "Mocking new: allocating " << size << " bytes." << std::endl;
        return nullptr;
    }
    void *ptr = std::malloc(size);
    if (!ptr) {
        return nullptr;
    }
    return ptr;
}

TEST_F(TestSmapHandlerMsg, SmapMigrateOutCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    MigrateOutMsg msg = {0};
    int pidType;

    SmapMigrateOutCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, &msg, pidType);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1)).then(returnValue(0)).then(returnValue(NUM_2));

    result = codec.EncodeRequest(inputBuffer, &msg, pidType);
    EXPECT_EQ(result, 1);

    result = codec.EncodeRequest(inputBuffer, &msg, pidType);
    EXPECT_EQ(result, NUM_2);
}

TEST_F(TestSmapHandlerMsg, SmapMigrateOutCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    MigrateOutMsg msg = {0};
    int pidType;

    SmapMigrateOutCodec codec;
    int result = codec.DecodeRequest(inputBuffer, msg, pidType);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(TestSmapHandlerMsg, SmapMigrateOutCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapMigrateOutCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapMigrateBackCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    MigrateBackMsg msg = {0};

    SmapMigrateBackCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, &msg);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeRequest(inputBuffer, &msg);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapMigrateBackCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    MigrateBackMsg msg = {0};

    SmapMigrateBackCodec codec;
    int result = codec.DecodeRequest(inputBuffer, msg);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(TestSmapHandlerMsg, SmapMigrateBackCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapMigrateBackCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapRemoveCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    RemoveMsg msg = {0};
    int pidType;

    SmapRemoveCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, &msg, pidType);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1)).then(returnValue(0)).then(returnValue(NUM_2));

    result = codec.EncodeRequest(inputBuffer, &msg, pidType);
    EXPECT_EQ(result, 1);

    result = codec.EncodeRequest(inputBuffer, &msg, pidType);
    EXPECT_EQ(result, NUM_2);
}

TEST_F(TestSmapHandlerMsg, SmapRemoveCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    RemoveMsg msg = {0};
    int pidType;

    SmapRemoveCodec codec;
    int result = codec.DecodeRequest(inputBuffer, msg, pidType);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(TestSmapHandlerMsg, SmapRemoveCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapRemoveCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapEnableNodeCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    EnableNodeMsg msg = {0};
    int pidType;

    SmapEnableNodeCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, &msg);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeRequest(inputBuffer, &msg);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapEnableNodeCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    EnableNodeMsg msg = {0};

    SmapEnableNodeCodec codec;
    int result = codec.DecodeRequest(inputBuffer, msg);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(TestSmapHandlerMsg, SmapEnableNodeCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapEnableNodeCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapInitCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    uint32_t pageType;

    SmapInitCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, pageType);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeRequest(inputBuffer, pageType);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapInitCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    uint32_t pageType;

    SmapInitCodec codec;
    int result = codec.DecodeRequest(inputBuffer, pageType);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(TestSmapHandlerMsg, SmapInitCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapInitCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapInitCodecDecodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    SmapInitCodec codec;

    int result = codec.DecodeResponse(inputBuffer);
    EXPECT_EQ(result, IPC_ERROR);
}

TEST_F(TestSmapHandlerMsg, SmapStopCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    SmapStopCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;
}

TEST_F(TestSmapHandlerMsg, SmapStopCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    SmapStopCodec codec;
    int result = codec.DecodeRequest(inputBuffer);
    EXPECT_EQ(result, 0);
}

TEST_F(TestSmapHandlerMsg, SmapStopCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapStopCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapUrgentMigrateOutCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    uint64_t size;

    SmapUrgentMigrateOutCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, size);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeRequest(inputBuffer, size);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapUrgentMigrateOutCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    uint64_t size;

    SmapUrgentMigrateOutCodec codec;
    int result = codec.DecodeRequest(inputBuffer, size);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(TestSmapHandlerMsg, SmapUrgentMigrateOutCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;

    SmapUrgentMigrateOutCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;
}

TEST_F(TestSmapHandlerMsg, SmapUrgentMigrateOutCodecDecodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    SmapUrgentMigrateOutCodec codec;
    int result = codec.DecodeResponse(inputBuffer);
    EXPECT_EQ(result, 0);
}

TEST_F(TestSmapHandlerMsg, SmapAddProcessTrackingCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    pid_t pidArr;
    uint32_t scanTime;
    uint32_t duration;
    int len = 1;
    int scanType;

    SmapAddProcessTrackingCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, &pidArr, &scanTime, &duration, len, scanType);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s)
        .stubs()
        .will(returnValue(1))
        .then(returnValue(0))
        .then(returnValue(NUM_2))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(NUM_3))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(NUM_4))
        .then(returnValue(NUM_4))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(NUM_5));

    result = codec.EncodeRequest(inputBuffer, &pidArr, &scanTime, &duration, len, scanType);
    EXPECT_EQ(result, 1);

    result = codec.EncodeRequest(inputBuffer, &pidArr, &scanTime, &duration, len, scanType);
    EXPECT_EQ(result, NUM_2);

    result = codec.EncodeRequest(inputBuffer, &pidArr, &scanTime, &duration, len, scanType);
    EXPECT_EQ(result, NUM_3);

    result = codec.EncodeRequest(inputBuffer, &pidArr, &scanTime, &duration, len, scanType);
    EXPECT_EQ(result, NUM_4);

    result = codec.EncodeRequest(inputBuffer, &pidArr, &scanTime, &duration, len, scanType);
    EXPECT_EQ(result, NUM_5);
}

TEST_F(TestSmapHandlerMsg, SmapAddProcessTrackingCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    uint32_t scanTime[] = {100, 200, 300};
    uint32_t duration[] = {100, 200, 300};
    int len = 3;
    int bufferlen;
    int scanType = 1;

    SmapAddProcessTrackingCodec codec;
    codec.EncodeRequest(inputBuffer, pidArr, scanTime, duration, len, scanType);
    bufferlen = inputBuffer.len;
    inputBuffer.len = 0;
    int result = codec.DecodeRequest(inputBuffer, pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(result, -EINVAL);

    inputBuffer.len = sizeof(int);
    result = codec.DecodeRequest(inputBuffer, pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(result, -EINVAL);

    inputBuffer.len = bufferlen;
    MOCKER(memcpy_s)
        .stubs()
        .will(returnValue(1))
        .then(returnValue(0))
        .then(returnValue(NUM_2))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(NUM_3));

    result = codec.DecodeRequest(inputBuffer, pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(result, 1);

    result = codec.DecodeRequest(inputBuffer, pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(result, NUM_2);

    result = codec.DecodeRequest(inputBuffer, pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(result, NUM_3);
}

TEST_F(TestSmapHandlerMsg, SmapAddProcessTrackingCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapAddProcessTrackingCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapRemoveProcessTrackingCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int flag = 1;

    SmapRemoveProcessTrackingCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, pidArr, len, flag);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s)
        .stubs()
        .will(returnValue(1))
        .then(returnValue(0))
        .then(returnValue(NUM_2))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(NUM_3));

    result = codec.EncodeRequest(inputBuffer, pidArr, len, flag);
    EXPECT_EQ(result, 1);

    result = codec.EncodeRequest(inputBuffer, pidArr, len, flag);
    EXPECT_EQ(result, NUM_2);

    result = codec.EncodeRequest(inputBuffer, pidArr, len, flag);
    EXPECT_EQ(result, NUM_3);
}

TEST_F(TestSmapHandlerMsg, SmapRemoveProcessTrackingCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int flag = 1;

    SmapRemoveProcessTrackingCodec codec;
    codec.EncodeRequest(inputBuffer, pidArr, len, flag);
    inputBuffer.len = 0;
    int result = codec.DecodeRequest(inputBuffer, pidArr, len, flag);
    EXPECT_EQ(result, -EINVAL);

    inputBuffer.len = sizeof(int);
    result = codec.DecodeRequest(inputBuffer, pidArr, len, flag);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(TestSmapHandlerMsg, SmapRemoveProcessTrackingCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapRemoveProcessTrackingCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}

TEST_F(TestSmapHandlerMsg, SmapEnableProcessMigrateCodecEncodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int enable = 1;
    int flags = 1;

    SmapEnableProcessMigrateCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeRequest(inputBuffer, pidArr, len, enable, flags);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s)
        .stubs()
        .will(returnValue(1))
        .then(returnValue(0))
        .then(returnValue(NUM_2))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(NUM_3))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(0))
        .then(returnValue(NUM_4));

    result = codec.EncodeRequest(inputBuffer, pidArr, len, enable, flags);
    EXPECT_EQ(result, 1);

    result = codec.EncodeRequest(inputBuffer, pidArr, len, enable, flags);
    EXPECT_EQ(result, NUM_2);

    result = codec.EncodeRequest(inputBuffer, pidArr, len, enable, flags);
    EXPECT_EQ(result, NUM_3);

    result = codec.EncodeRequest(inputBuffer, pidArr, len, enable, flags);
    EXPECT_EQ(result, NUM_4);
}

TEST_F(TestSmapHandlerMsg, SmapEnableProcessMigrateCodecDecodeRequestFailed)
{
    TurboByteBuffer inputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int enable = 1;
    int flags = 1;

    SmapEnableProcessMigrateCodec codec;
    codec.EncodeRequest(inputBuffer, pidArr, len, enable, flags);
    inputBuffer.len = 0;
    int result = codec.DecodeRequest(inputBuffer, pidArr, len, enable, flags);
    EXPECT_EQ(result, -EINVAL);

    inputBuffer.len = sizeof(int);
    result = codec.DecodeRequest(inputBuffer, pidArr, len, enable, flags);
    EXPECT_EQ(result, -EINVAL);
}

TEST_F(TestSmapHandlerMsg, SmapEnableProcessMigrateCodecEncodeResponseFailed)
{
    TurboByteBuffer inputBuffer;
    int ret;

    SmapEnableProcessMigrateCodec codec;
    g_mock_new_should_fail = true;
    int result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, -EINVAL);
    g_mock_new_should_fail = false;

    MOCKER(memcpy_s).stubs().will(returnValue(1));

    result = codec.EncodeResponse(inputBuffer, ret);
    EXPECT_EQ(result, 1);
}