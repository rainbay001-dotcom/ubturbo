/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.h>
#include <mockcpp/mokc.h>
#include <cerrno>
#include <unistd.h>
#include <dlfcn.h>
#include "turbo_ipc_server.h"
#include "turbo_error.h"
#include "smap_interface.h"
#include "smap_handler_msg.h"
#include "turbo_module_smap.h"
#include "ulog.h"
#include <fstream>

constexpr int NUM_MINUS_2 = -2;
constexpr int NUM_2 = 2;
constexpr int NUM_3 = 3;
void *dlopen(const char *__file, int __mode);
int dlclose(void *__handle);
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace turbo::smap::codec;
using namespace turbo::ipc::server;
using namespace turbo::smap;

class TestTurboModuleSmap : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestTurboModuleSmap, SmapMigrateOutHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    MigrateOutMsg msg = { 0 };
    msg.count = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;
    int pidType = 1;

    StubSmapPtr();
    SmapMigrateOutCodec codec;
    codec.EncodeRequest(inputBuffer, &msg, pidType);
    RetCode result = SmapMigrateOutHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapMigrateOutHandlerEncodeResponseFailed)
{
    int ret;
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    MigrateOutMsg msg = { 0 };
    msg.count = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;
    int pidType = 1;

    MOCKER_CPP(&SmapMigrateOutCodec::DecodeRequest, int(*)(const TurboByteBuffer &buffer,
        MigrateOutMsg &msg, int &pidType))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));

    MOCKER_CPP(&SmapMigrateOutCodec::EncodeResponse, int(*)(TurboByteBuffer &buffer,
        int returnValue))
        .stubs()
        .will(returnValue(1));

    ret = SmapMigrateOutHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);

    ret = SmapMigrateOutHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);
}

TEST_F(TestTurboModuleSmap, SmapMigrateBackHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    MigrateBackMsg msg = { 0 } ;
    msg.count = 1;
    msg.payload[0].srcNid = 4;
    msg.payload[0].destNid = 0;

    StubSmapPtr();
    SmapMigrateBackCodec codec;
    codec.EncodeRequest(inputBuffer, &msg);
    RetCode result = SmapMigrateBackHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapMigrateBackHandlerEncodeResponseFailed)
{
    int ret;
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    MigrateBackMsg msg = { 0 } ;
    msg.count = 1;
    msg.payload[0].srcNid = 4;
    msg.payload[0].destNid = 0;

    MOCKER_CPP(&SmapMigrateBackCodec::DecodeRequest, int(*)(const TurboByteBuffer &buffer,
        MigrateBackMsg &msg))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));

    MOCKER_CPP(&SmapMigrateBackCodec::EncodeResponse, int(*)(TurboByteBuffer &buffer,
        int returnValue))
        .stubs()
        .will(returnValue(1));

    ret = SmapMigrateBackHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);

    ret = SmapMigrateBackHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);
}

TEST_F(TestTurboModuleSmap, SmapRemoveHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    RemoveMsg msg = { 0 };
    msg.count = 1;
    int pidType = 1;

    StubSmapPtr();
    SmapRemoveCodec codec;
    codec.EncodeRequest(inputBuffer, &msg, pidType);
    RetCode result = SmapRemoveHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapRemoveHandEncodeResponseFailed)
{
    int ret;
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    RemoveMsg msg = { 0 };
    msg.count = 1;
    int pidType = 1;

    MOCKER_CPP(&SmapRemoveCodec::DecodeRequest, int(*)(const TurboByteBuffer &buffer,
        RemoveMsg &msg, int &pidType))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));

    MOCKER_CPP(&SmapRemoveCodec::EncodeResponse, int(*)(TurboByteBuffer &buffer,
        int returnValue))
        .stubs()
        .will(returnValue(1));

    ret = SmapRemoveHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);

    ret = SmapRemoveHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);
}

TEST_F(TestTurboModuleSmap, SmapEnableNodeHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    EnableNodeMsg msg{};
    msg.nid = 4;
    msg.enable = 1;

    StubSmapPtr();
    SmapEnableNodeCodec codec;
    codec.EncodeRequest(inputBuffer, &msg);
    RetCode result = SmapEnableNodeHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapEnableNodeHandlerEncodeResponseFailed)
{
    int ret;
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    EnableNodeMsg msg{};
    msg.nid = 4;
    msg.enable = 1;

    MOCKER_CPP(&SmapEnableNodeCodec::DecodeRequest, int(*)(const TurboByteBuffer &buffer,
        EnableNodeMsg &msg))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));

    MOCKER_CPP(&SmapEnableNodeCodec::EncodeResponse, int(*)(TurboByteBuffer &buffer,
        int returnValue))
        .stubs()
        .will(returnValue(1));

    ret = SmapEnableNodeHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);

    ret = SmapEnableNodeHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);
}

namespace turbo {
namespace smap {
void SmapToTurboLog(int level, const char *str, const char *moduleName);
void SmapHandlerMsgToTurboLog(int level, const char *str, const char *moduleName);
}
}

TEST_F(TestTurboModuleSmap, SmapToTurboLogTest)
{
    const char* moduleName = "TestModule";
    const char* logMessage = "Test log message";

    int level = static_cast<int>(LoggerLevel::LOGGER_INFO_LEVEL);
    SmapToTurboLog(level, logMessage, moduleName);

    level = static_cast<int>(LoggerLevel::LOGGER_WARNING_LEVEL);
    SmapToTurboLog(level, logMessage, moduleName);

    level = static_cast<int>(LoggerLevel::LOGGER_ERROR_LEVEL);
    SmapToTurboLog(level, logMessage, moduleName);

    level = 999;
    SmapToTurboLog(level, logMessage, moduleName);
}

TEST_F(TestTurboModuleSmap, SmapHandlerMsgToTurboLogTest)
{
    const char* moduleName = "TestModule";
    const char* logMessage = "Test log message";

    int level = static_cast<int>(LoggerLevel::LOGGER_INFO_LEVEL);
    SmapHandlerMsgToTurboLog(level, logMessage, moduleName);

    level = static_cast<int>(LoggerLevel::LOGGER_WARNING_LEVEL);
    SmapHandlerMsgToTurboLog(level, logMessage, moduleName);

    level = static_cast<int>(LoggerLevel::LOGGER_ERROR_LEVEL);
    SmapHandlerMsgToTurboLog(level, logMessage, moduleName);

    level = 999;
    SmapHandlerMsgToTurboLog(level, logMessage, moduleName);
}

TEST_F(TestTurboModuleSmap, SmapInitHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    uint32_t pageType = 1;

    StubSmapPtr();
    SmapInitCodec codec;
    codec.EncodeRequest(inputBuffer, pageType);
    RetCode result = SmapInitHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapInitHandlerEncodeResponseFailed)
{
    int ret;
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    uint32_t pageType = 1;

    MOCKER_CPP(&SmapInitCodec::DecodeRequest, int(*)(const TurboByteBuffer &buffer,
        uint32_t &pageType))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));

    MOCKER_CPP(&SmapInitCodec::EncodeResponse, int(*)(TurboByteBuffer &buffer,
        int returnValue))
        .stubs()
        .will(returnValue(1));

    ret = SmapInitHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);

    ret = SmapInitHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);
}

TEST_F(TestTurboModuleSmap, SmapStopHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    SmapStopCodec codec;
    RetCode result = SmapStopHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapStopHandlerEncodeResponseFailed)
{
    int ret;
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    MOCKER_CPP(&SmapStopCodec::EncodeResponse, int(*)(TurboByteBuffer &buffer,
        int returnValue))
        .stubs()
        .will(returnValue(1));

    ret = SmapStopHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);
}

TEST_F(TestTurboModuleSmap, SmapUrgentMigrateOutHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    uint64_t size = 1024;

    StubSmapPtr();
    SmapUrgentMigrateOutCodec codec;
    codec.EncodeRequest(inputBuffer, size);
    RetCode result = SmapUrgentMigrateOutHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    // 由于 ubturbo_smap_urgent_migrate_out 是 void 函数，这里不进行返回值检查
}

TEST_F(TestTurboModuleSmap, SmapUrgentMigrateOutHandlerDecodeResponseFailed)
{
    int ret;
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    uint64_t size = 1024;

    MOCKER_CPP(&SmapUrgentMigrateOutCodec::DecodeRequest, int(*)(const TurboByteBuffer &buffer,
        uint64_t &size))
        .stubs()
        .will(returnValue(1));

    ret = SmapUrgentMigrateOutHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(1, ret);
}

TEST_F(TestTurboModuleSmap, SmapUrgentMigrateOutHandlerEncodeResponseFailed)
{
    int ret;
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    uint64_t size = 1024;

    MOCKER_CPP(&SmapUrgentMigrateOutCodec::DecodeRequest, int(*)(const TurboByteBuffer &buffer,
        uint64_t &size))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&SmapUrgentMigrateOutCodec::EncodeResponse, int(*)(TurboByteBuffer &buffer))
        .stubs()
        .will(returnValue(1));

    ret = SmapUrgentMigrateOutHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(0, ret);
}

TEST_F(TestTurboModuleSmap, SmapAddProcessTrackingHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    uint32_t scanTime[] = {100, 200, 300};
    uint32_t duration[] = {100, 200, 300};
    int len = 3;
    int scanType = 1;

    StubSmapPtr();
    SmapAddProcessTrackingCodec codec;
    codec.EncodeRequest(inputBuffer, pidArr, scanTime, duration, len, scanType);
    RetCode result = SmapAddProcessTrackingHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapAddProcessTrackingHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    MOCKER_CPP(&SmapAddProcessTrackingCodec::DecodeRequest,
               int (*)(const TurboByteBuffer &buffer, pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int &len,
                       int &scanType))
        .stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    RetCode result = SmapAddProcessTrackingHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SmapAddProcessTrackingCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapAddProcessTrackingHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapRemoveProcessTrackingHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int flag = 1;

    StubSmapPtr();
    SmapRemoveProcessTrackingCodec codec;
    codec.EncodeRequest(inputBuffer, pidArr, len, flag);
    RetCode result = SmapRemoveProcessTrackingHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapRemoveProcessTrackingHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    MOCKER_CPP(&SmapRemoveProcessTrackingCodec::DecodeRequest,
               int (*)(const TurboByteBuffer &buffer, pid_t *pidArr, int &len, int &flag))
        .stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    RetCode result = SmapRemoveProcessTrackingHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SmapRemoveProcessTrackingCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapRemoveProcessTrackingHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapEnableProcessMigrateHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int enable = 1;
    int flags = 1;

    StubSmapPtr();
    SmapEnableProcessMigrateCodec codec;
    codec.EncodeRequest(inputBuffer, pidArr, len, enable, flags);
    RetCode result = SmapEnableProcessMigrateHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapEnableProcessMigrateHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    MOCKER_CPP(&SmapEnableProcessMigrateCodec::DecodeRequest,
               int (*)(const TurboByteBuffer &buffer, pid_t *pidArr, int &len, int &enable, int &flags))
        .stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    RetCode result = SmapEnableProcessMigrateHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SmapEnableProcessMigrateCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapEnableProcessMigrateHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SetSmapRemoteNumaInfoHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    SetRemoteNumaInfoMsg msg = { 0 };
    msg.srcNid = 0;
    msg.destNid = 4;
    msg.size = 100;

    StubSmapPtr();
    SetSmapRemoteNumaInfoCodec codec;
    codec.EncodeRequest(inputBuffer, &msg);
    RetCode result = SetSmapRemoteNumaInfoHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SetSmapRemoteNumaInfoHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    MOCKER_CPP(&SetSmapRemoteNumaInfoCodec::DecodeRequest,
               int (*)(const TurboByteBuffer &buffer, SetRemoteNumaInfoMsg &msg))
        .stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    RetCode result = SetSmapRemoteNumaInfoHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SetSmapRemoteNumaInfoCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SetSmapRemoteNumaInfoHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapQueryVmFreqHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    int pid = 1;
    uint16_t lengthIn = 10;

    StubSmapPtr();
    SmapQueryVmFreqCodec codec;
    codec.EncodeRequest(inputBuffer, pid, lengthIn, 0);
    RetCode result = SmapQueryVmFreqHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);
}

TEST_F(TestTurboModuleSmap, SmapQueryVmFreqHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    MOCKER_CPP(&SmapQueryVmFreqCodec::DecodeRequest,
               int (*)(const TurboByteBuffer &buffer, int &pid, uint16_t &lengthIn, int &dataSource))
        .stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    RetCode result = SmapQueryVmFreqHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SmapQueryVmFreqCodec::EncodeResponse,
               int (*)(TurboByteBuffer &buffer, uint16_t * data, uint16_t lengthOut, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapQueryVmFreqHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SetSmapRunModeHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    int runMode = 1;

    StubSmapPtr();
    SetSmapRunModeCodec codec;
    codec.EncodeRequest(inputBuffer, runMode);
    RetCode result = SetSmapRunModeHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SetSmapRunModeHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    MOCKER_CPP(&SetSmapRunModeCodec::DecodeRequest, int (*)(const TurboByteBuffer &buffer, int &runMode))
        .stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    RetCode result = SetSmapRunModeHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SetSmapRunModeCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SetSmapRunModeHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapIsRunningHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    RetCode result = SmapIsRunningHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    SmapIsRunningCodec codec;
    bool ret = codec.DecodeResponse(outputBuffer);
    EXPECT_TRUE(ret);
}

TEST_F(TestTurboModuleSmap, SmapIsRunningHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    StubSmapPtr();
    MOCKER_CPP(&SmapIsRunningCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, bool returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    RetCode result = SmapIsRunningHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapMigrateOutSyncHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    MigrateOutMsg msg = { 0 };
    msg.count = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;
    int pidType = 1;
    uint64_t maxWaitTime = 1000;

    StubSmapPtr();
    SmapMigrateOutSyncCodec codec;
    codec.EncodeRequest(inputBuffer, &msg, pidType, maxWaitTime);
    RetCode result = SmapMigrateOutSyncHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapMigrateOutSyncHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    MOCKER_CPP(&SmapMigrateOutSyncCodec::DecodeRequest,
               int (*)(const TurboByteBuffer &buffer, MigrateOutMsg &msg, int &pidType, uint64_t &maxWaitTime))
        .stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    RetCode result = SmapMigrateOutSyncHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SmapMigrateOutSyncCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapMigrateOutSyncHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapMigrateRemoteNumaHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    MigrateNumaMsg msg = { 0 };
    msg.count = 1;
    msg.srcNid = 4;
    msg.destNid = 5;

    StubSmapPtr();
    SmapMigrateRemoteNumaCodec codec;
    codec.EncodeRequest(inputBuffer, &msg);
    RetCode result = SmapMigrateRemoteNumaHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapMigrateRemoteNumaHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;

    StubSmapPtr();
    MOCKER_CPP(&SmapMigrateRemoteNumaCodec::DecodeRequest, int (*)(const TurboByteBuffer &buffer, MigrateNumaMsg &msg))
        .stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    RetCode result = SmapMigrateRemoteNumaHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SmapMigrateRemoteNumaCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapMigrateRemoteNumaHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapMigratePidRemoteNumaHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int srcNid = 0;
    int destNid = 1;

    StubSmapPtr();
    SmapMigratePidRemoteNumaCodec codec;
    codec.EncodeRequest(inputBuffer, pidArr, len, srcNid, destNid);
    RetCode result = SmapMigratePidRemoteNumaHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret = codec.DecodeResponse(outputBuffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapMigratePidRemoteNumaHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int srcNid = 0;
    int destNid = 1;

    StubSmapPtr();
    SmapMigratePidRemoteNumaCodec codec;
    codec.EncodeRequest(inputBuffer, pidArr, len, srcNid, destNid);
    MOCKER_CPP(&SmapMigratePidRemoteNumaCodec::EncodeResponse, int (*)(TurboByteBuffer &buffer, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    RetCode result = SmapMigratePidRemoteNumaHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SmapMigratePidRemoteNumaCodec::DecodeRequest,
               int (*)(const TurboByteBuffer &buffer, pid_t *&pidArr, int &len, int &srcNid, int &destNid))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapMigratePidRemoteNumaHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapQueryProcessConfigHandlerTest)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    int nid = 1;
    struct ProcessPayload payload = { 0 };
    int inLen = 1;
    int outLen;

    StubSmapPtr();
    SmapQueryProcessConfigCodec codec;
    codec.EncodeRequest(inputBuffer, nid, inLen);
    RetCode result = SmapQueryProcessConfigHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    int ret;
    codec.DecodeResponse(outputBuffer, &payload, outLen, ret);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, SmapQueryProcessConfigHandlerLenLessZero)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    int nid = 1;
    struct ProcessPayload payload = { 0 };
    int inLen = -1;
    int outLen;

    StubSmapPtr();
    SmapQueryProcessConfigCodec codec;
    codec.EncodeRequest(inputBuffer, nid, inLen);
    RetCode result = SmapQueryProcessConfigHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapQueryProcessConfigHandlerFailed)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    int nid = 1;
    struct ProcessPayload payload = { 0 };
    int inLen = 1;
    int outLen;

    StubSmapPtr();
    SmapQueryProcessConfigCodec codec;
    codec.EncodeRequest(inputBuffer, nid, inLen);
    MOCKER_CPP(&SmapQueryProcessConfigCodec::EncodeResponse,
               int (*)(TurboByteBuffer &buffer, struct ProcessPayload *result, int outLen, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    RetCode result = SmapQueryProcessConfigHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);

    MOCKER_CPP(&SmapQueryProcessConfigCodec::DecodeRequest,
               int (*)(const TurboByteBuffer &buffer, int &nid, int &inLen))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapQueryProcessConfigHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, SmapQueryRemoteNumaFreqHandler)
{
    TurboByteBuffer inputBuffer;
    TurboByteBuffer outputBuffer;
    uint16_t numa;
    uint16_t length = 1;

    StubSmapPtr();
    SmapQueryRemoteNumaFreqCodec codec;
    codec.EncodeRequest(inputBuffer, &numa, length);
    RetCode result = SmapQueryRemoteNumaFreqHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_OK);

    MOCKER_CPP(&SmapQueryRemoteNumaFreqCodec::EncodeResponse,
               int (*)(TurboByteBuffer &buffer, uint64_t *freq, uint16_t len, int returnValue))
        .stubs()
        .will(returnValue(-EINVAL));
    result = SmapQueryRemoteNumaFreqHandler(inputBuffer, outputBuffer);
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, OpenSmapHandlerTest)
{
    MOCKER(dlopen).stubs().will(returnValue((void *)nullptr));
    int ret = OpenSmapHandler();
    EXPECT_EQ(ret, -ENOENT);
}

TEST_F(TestTurboModuleSmap, ReturnZeroWhenAllSymbolsFound)
{
    MOCKER((void* (*)(const char*, int))(dlopen))
        .stubs()
        .will(returnValue((void *)0x1234));

    for (int i = 0; i < 18; ++i) {
        MOCKER((void* (*)(void*, const char*))(dlsym))
            .stubs()
            .will(returnValue((void *)0x5678));
    }

    int ret = OpenSmapHandler();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestTurboModuleSmap, ReturnEINVALWhenAnySymbolMissing)
{
    MOCKER((void* (*)(const char*, int))(dlopen))
        .stubs()
        .will(returnValue((void *)0x1234));

    MOCKER((void* (*)(void*, const char*))(dlsym))
        .stubs()
        .will(returnValue((void *)nullptr));

    for (int i = 1; i < 18; ++i) {
        MOCKER((void* (*)(void*, const char*))(dlsym))
            .stubs()
            .will(returnValue((void *)0x5678));
    }

    int ret = OpenSmapHandler();
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(TestTurboModuleSmap, CloseSmapHandlerTest)
{
    MOCKER((int (*)(void*))(dlclose))
        .stubs()
        .will(returnValue(0));

    CloseSmapHandler();
}

TEST_F(TestTurboModuleSmap, RegSmapHandlerTest)
{
    MOCKER(UBTurboRegIpcService).stubs().will(returnValue(0));
    RegSmapHandler();
}

TEST_F(TestTurboModuleSmap, UnRegSmapHandlerTest)
{
    MOCKER(UBTurboUnRegIpcService).stubs().will(returnValue(0));
    UnRegSmapHandler();
}

TEST_F(TestTurboModuleSmap, InitTest)
{
    TurboModuleSmap module;
    RetCode result = module.Init();
    EXPECT_EQ(result, TURBO_OK);
}

TEST_F(TestTurboModuleSmap, StartTest)
{
    TurboModuleSmap module;
    MOCKER(OpenSmapHandler).stubs().will(returnValue(0));
    MOCKER(RegSmapHandler).stubs();
    MOCKER_CPP(&turbo::smap::ulog::UpstreamSubscribeLogger, void (*)(Logfunc extlog)).stubs();
    MOCKER_CPP(&TurboModuleSmap::PageTypeFileExists, bool (*)())
        .stubs()
        .will(repeat(true, NUM_3))
        .then(returnValue(false));
    MOCKER_CPP(&TurboModuleSmap::LoadPageType, RetCode (*)(uint32_t &pageType))
        .stubs()
        .will(repeat(TURBO_OK, NUM_2))
        .then(returnValue(TURBO_ERROR));

    StubSmapPtr();
    MOCKER(StubSmapInit)
        .stubs()
        .will(returnValue(0))
        .then(returnValue(-1));
    RetCode result = module.Start();
    EXPECT_EQ(result, TURBO_OK);

    result = module.Start();
    EXPECT_EQ(result, TURBO_OK);

    result = module.Start();
    EXPECT_EQ(result, TURBO_OK);

    result = module.Start();
    EXPECT_EQ(result, TURBO_OK);
}

TEST_F(TestTurboModuleSmap, StartFailed)
{
    TurboModuleSmap module;
    MOCKER(OpenSmapHandler)
        .stubs()
        .will(returnValue(0))
        .then(returnValue(-EINVAL));
    MOCKER(RegSmapHandler).stubs();
    MOCKER_CPP(&turbo::smap::ulog::UpstreamSubscribeLogger, void (*)(Logfunc extlog)).stubs();
    MOCKER_CPP(&TurboModuleSmap::PageTypeFileExists, bool (*)())
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&TurboModuleSmap::LoadPageType, RetCode (*)(uint32_t &pageType))
        .stubs()
        .will(returnValue(TURBO_OK));

    StubSmapPtr();
    MOCKER(StubSmapInit)
        .stubs()
        .will(returnValue(NUM_MINUS_2));

    RetCode result = module.Start();
    EXPECT_EQ(result, TURBO_ERROR);

    result = module.Start();
    EXPECT_EQ(result, TURBO_ERROR);
}

TEST_F(TestTurboModuleSmap, StopTest)
{
    TurboModuleSmap module;
    MOCKER(UnRegSmapHandler).stubs();
    module.Stop();
}

TEST_F(TestTurboModuleSmap, UnInitTest)
{
    TurboModuleSmap module;
    module.UnInit();
}

TEST_F(TestTurboModuleSmap, NameTest)
{
    TurboModuleSmap module;
    std::string name = module.Name();
    EXPECT_EQ(name, "Smap");
}

TEST_F(TestTurboModuleSmap, LoadPageTypeTest)
{
    TurboModuleSmap module;
    uint32_t pageType;
    RetCode ret = module.LoadPageType(pageType);
    EXPECT_EQ(ret, TURBO_OK);
}

TEST_F(TestTurboModuleSmap, PageTypeFileExistsTest)
{
    TurboModuleSmap module;
    RetCode ret = module.PageTypeFileExists();
    EXPECT_EQ(ret, TURBO_ERROR);
}
