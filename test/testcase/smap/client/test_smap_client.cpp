/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.h>
#include <mockcpp/mokc.h>
#include <cerrno>

#include "turbo_ipc_client.h"
#include "smap_interface.h"
#include "smap_handler_msg.h"
#include "ulog.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace turbo::smap::codec;
using namespace turbo::ipc::client;

class TestSmapClient : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

uint32_t Test_UBTurboFunctionCaller(const std::string &function, const TurboByteBuffer &params, TurboByteBuffer &result)
{
    uint8_t *num = new uint8_t;
    *num = 0;
    result.data = num;
    result.len = 1;
    result.freeFunc = nullptr;
    return 0;
}

TEST_F(TestSmapClient, SmapMigrateOutTest)
{
    struct MigrateOutMsg msg = { 0 };

    msg.count = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_migrate_out(&msg, 1);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapMigrateOutNULLMsg)
{
    int ret;
    struct MigrateOutMsg *msg = NULL;
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    
    ret = ubturbo_smap_migrate_out(msg, 1);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapMigrateOutEncodeRequestFailed)
{
    int ret;
    struct MigrateOutMsg msg = { 0 };

    msg.count = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;

    MOCKER_CPP(&SmapMigrateOutCodec::EncodeRequest, int(*)(SmapMigrateOutCodec*, TurboByteBuffer&,
        struct MigrateOutMsg*, int))
        .stubs()
        .will(returnValue(1));
    
    ret = ubturbo_smap_migrate_out(&msg, 1);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapMigrateOutUBTurboFunctionCallerFailed)
{
    int ret;
    struct MigrateOutMsg msg = { 0 };

    msg.count = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;

    MOCKER_CPP(&SmapMigrateOutCodec::EncodeRequest, int(*)(SmapMigrateOutCodec*, TurboByteBuffer&,
        struct MigrateOutMsg*, int))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(1));
    
    ret = ubturbo_smap_migrate_out(&msg, 1);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapMigrateOutDecodeResponseFailed)
{
    int ret;
    struct MigrateOutMsg msg = { 0 };

    msg.count = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].pid = 1;
    msg.payload[0].ratio = 25;

    MOCKER_CPP(&SmapMigrateOutCodec::EncodeRequest, int(*)(SmapMigrateOutCodec*, TurboByteBuffer&,
        struct MigrateOutMsg*, int))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&SmapMigrateOutCodec::DecodeResponse, int(*)(SmapMigrateOutCodec *, TurboByteBuffer &))
        .stubs()
        .will(returnValue(1));
    
    ret = ubturbo_smap_migrate_out(&msg, 1);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapMigrateBackTest)
{
    struct MigrateBackMsg msg;
    msg.count = 1;
    msg.payload[0].srcNid = 4;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_migrate_back(&msg);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapMigrateBackNULLMsg)
{
    int ret;
    struct MigrateBackMsg *msg = nullptr;

    ret = ubturbo_smap_migrate_back(msg);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapMigrateBackEncodeRequestFailed)
{
    int ret;
    struct MigrateBackMsg msg;
    msg.count = 1;
    msg.payload[0].srcNid = 4;

    MOCKER_CPP(&SmapMigrateBackCodec::EncodeRequest, int(*)(SmapMigrateBackCodec*, TurboByteBuffer &,
        MigrateBackMsg *))
        .stubs()
        .will(returnValue(1));
    
    ret = ubturbo_smap_migrate_back(&msg);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapMigrateBackUBTurboFunctionCallerFailed)
{
    int ret;
    struct MigrateBackMsg msg;
    msg.count = 1;
    msg.payload[0].srcNid = 4;

    MOCKER_CPP(&SmapMigrateBackCodec::EncodeRequest, int(*)(SmapMigrateBackCodec*, TurboByteBuffer &,
        MigrateBackMsg *))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(3));

    ret = ubturbo_smap_migrate_back(&msg);
    EXPECT_EQ(3, ret);
}

TEST_F(TestSmapClient, SmapMigrateBackDecodeFailed)
{
    int ret;
    struct MigrateBackMsg msg;
    msg.count = 1;
    msg.payload[0].srcNid = 4;

    MOCKER_CPP(&SmapMigrateBackCodec::EncodeRequest, int(*)(SmapMigrateBackCodec*, TurboByteBuffer &,
        MigrateBackMsg *msg))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&SmapMigrateBackCodec::DecodeResponse, int(*)(SmapMigrateBackCodec *, TurboByteBuffer &))
        .stubs()
        .will(returnValue(1));
    
    ret = ubturbo_smap_migrate_back(&msg);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapRemoveTest)
{
    struct RemoveMsg msg;
    msg.count = 1;
    int pidType = 1;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_remove(&msg, pidType);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapRemoveNULLMsg)
{
    int ret;
    int pidType = 1;
    struct RemoveMsg *msg = nullptr;

    ret = ubturbo_smap_remove(msg, pidType);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapRemoveEncodeRequestFailed)
{
    int ret;
    struct RemoveMsg msg;
    msg.count = 1;
    int pidType = 1;

    MOCKER_CPP(&SmapRemoveCodec::EncodeRequest, int(*)(SmapRemoveCodec*, TurboByteBuffer &,
        RemoveMsg *, int))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_remove(&msg, pidType);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapRemoveUBTurboFunctionCallerFailed)
{
    int ret;
    struct RemoveMsg msg;
    msg.count = 1;
    int pidType = 1;

    MOCKER_CPP(&SmapRemoveCodec::EncodeRequest, int(*)(SmapRemoveCodec*, TurboByteBuffer &,
        RemoveMsg *, int))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(1));

    ret = ubturbo_smap_remove(&msg, pidType);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapRemoveDecodeResponseFailed)
{
    int ret;
    struct RemoveMsg msg;
    msg.count = 1;
    int pidType = 1;

    MOCKER_CPP(&SmapRemoveCodec::EncodeRequest, int(*)(SmapRemoveCodec*, TurboByteBuffer &,
        RemoveMsg *, int))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&SmapRemoveCodec::DecodeResponse, int(*)(SmapRemoveCodec *,
        TurboByteBuffer &))
        .stubs()
        .will(returnValue(1));

    ret = ubturbo_smap_remove(&msg, pidType);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapEnableNodeTest)
{
    struct EnableNodeMsg msg;
    msg.nid = 4;
    msg.enable = 1;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_node_enable(&msg);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapEnableNodeNULLMsg)
{
    int ret;
    struct EnableNodeMsg *msg = nullptr;

    ret = ubturbo_smap_node_enable(msg);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapEnableNodeEncodeRequestFailed)
{
    int ret;
    struct EnableNodeMsg msg;
    msg.nid = 4;
    msg.enable = 1;

    MOCKER_CPP(&SmapEnableNodeCodec::EncodeRequest, int(*)(SmapEnableNodeCodec*, TurboByteBuffer &,
        EnableNodeMsg *))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_node_enable(&msg);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapEnableNodeUBTurboFunctionCallerFailed)
{
    int ret;
    struct EnableNodeMsg msg;
    msg.nid = 4;
    msg.enable = 1;

    MOCKER_CPP(&SmapEnableNodeCodec::EncodeRequest, int(*)(SmapEnableNodeCodec*, TurboByteBuffer &,
        EnableNodeMsg *))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(1));

    ret = ubturbo_smap_node_enable(&msg);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapEnableNodeDecodeResponseFailed)
{
    int ret;
    struct EnableNodeMsg msg;
    msg.nid = 4;
    msg.enable = 1;

    MOCKER_CPP(&SmapEnableNodeCodec::EncodeRequest, int(*)(SmapEnableNodeCodec*, TurboByteBuffer &buffer,
        EnableNodeMsg *))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&SmapEnableNodeCodec::DecodeResponse, int(*)(SmapEnableNodeCodec *, TurboByteBuffer &))
        .stubs()
        .will(returnValue(1));

    ret = ubturbo_smap_node_enable(&msg);
    EXPECT_EQ(1, ret);
}

extern int InitLog(Logfunc extlog);
TEST_F(TestSmapClient, InitLogTest)
{
    int ret;
    Logfunc extlog;

    MOCKER_CPP(&turbo::smap::ulog::UpstreamSubscribeLogger, void(*)(Logfunc extlog))
        .stubs()
        .will(returnValue(0));
    
    ret = InitLog(extlog);
    EXPECT_EQ(0, ret);
}

void MyLoggerFunc(int level, const char *str, const char *moduleName)
{
    return;
}

TEST_F(TestSmapClient, SmapInitTest)
{
    uint32_t pageType = 1;
    Logfunc extlog = nullptr;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_start(pageType, extlog);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapInitEncodeRequestFailed)
{
    int ret;
    uint32_t pageType = 1;
    Logfunc extlog = MyLoggerFunc;

    MOCKER_CPP(&SmapInitCodec::EncodeRequest, int(*)(SmapInitCodec*, TurboByteBuffer&, uint32_t))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_start(pageType, extlog);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapInitUBTurboFunctionCallerFailed)
{
    int ret;
    uint32_t pageType = 1;
    Logfunc extlog = MyLoggerFunc;

    MOCKER_CPP(&SmapInitCodec::EncodeRequest, int(*)(SmapInitCodec*, TurboByteBuffer&, uint32_t))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(1));

    ret = ubturbo_smap_start(pageType, extlog);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapInitDecodeResponseFailed)
{
    int ret;
    uint32_t pageType = 1;
    Logfunc extlog = MyLoggerFunc;

    MOCKER_CPP(&SmapInitCodec::EncodeRequest, int(*)(SmapInitCodec*, TurboByteBuffer&, uint32_t))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&SmapInitCodec::DecodeResponse, int(*)(SmapInitCodec *, TurboByteBuffer &))
        .stubs()
        .will(returnValue(1));

    ret = ubturbo_smap_start(pageType, extlog);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapStopTest)
{
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_stop();
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapStopEncodeRequestFailed)
{
    int ret;

    MOCKER_CPP(&SmapStopCodec::EncodeRequest, int(*)(SmapStopCodec*, TurboByteBuffer&))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_stop();
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapStopUBTurboFunctionCallerFailed)
{
    int ret;

    MOCKER_CPP(&SmapStopCodec::EncodeRequest, int(*)(SmapStopCodec*, TurboByteBuffer&))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(1));

    ret = ubturbo_smap_stop();
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapUrgentMigrateOutTest)
{
    uint64_t size = 1024;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));

    ubturbo_smap_urgent_migrate_out(size);
    // 由于 ubturbo_smap_urgent_migrate_out 是 void 函数，这里不进行返回值检查
}

TEST_F(TestSmapClient, SmapUrgentMigrateOutEncodeRequestFailed)
{
    int ret;
    uint64_t size = 1024;

    MOCKER_CPP(&SmapUrgentMigrateOutCodec::EncodeRequest, int(*)(SmapUrgentMigrateOutCodec*, TurboByteBuffer&,
        uint64_t))
        .stubs()
        .will(returnValue(-1));

    ubturbo_smap_urgent_migrate_out(size);
}

TEST_F(TestSmapClient, SmapUrgentMigrateOutUBTurboFunctionCallerFailed)
{
    int ret;
    uint64_t size = 1024;

    MOCKER_CPP(&SmapUrgentMigrateOutCodec::EncodeRequest, int(*)(SmapUrgentMigrateOutCodec*, TurboByteBuffer&,
        uint64_t))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(1));

    ubturbo_smap_urgent_migrate_out(size);
}

TEST_F(TestSmapClient, SmapAddProcessTrackingTest)
{
    pid_t pidArr[] = {1, 2, 3};
    uint32_t scanTime[] = {100, 200, 300};
    uint32_t duration[] = {100, 200, 300};
    int len = 3;
    int scanType = 1;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, len, scanType);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapAddProcessNullPidaddr)
{
    int ret;
    pid_t *pidArr = nullptr;
    uint32_t scanTime[] = {100, 200, 300};
    uint32_t duration[] = {100, 200, 300};
    int len = 3;
    int scanType = 1;

    ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapAddProcessErrLen)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    uint32_t scanTime[] = {100, 200, 300};
    uint32_t duration[] = {100, 200, 300};
    int len = -1;
    int scanType = 1;

    ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapAddProcessEncodeRequestFailed)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    uint32_t scanTime[] = {100, 200, 300};
    uint32_t duration[] = {100, 200, 300};
    int len = 3;
    int scanType = 1;

    MOCKER_CPP(&SmapAddProcessTrackingCodec::EncodeRequest, int(*)(SmapAddProcessTrackingCodec*, TurboByteBuffer &,
        pid_t *, uint32_t *, int, int))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapAddProcessUBTurboFunctionCallerFailed)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    uint32_t scanTime[] = {100, 200, 300};
    uint32_t duration[] = {100, 200, 300};
    int len = 3;
    int scanType = 1;

    MOCKER_CPP(&SmapAddProcessTrackingCodec::EncodeRequest, int(*)(SmapAddProcessTrackingCodec*, TurboByteBuffer &,
        pid_t *, uint32_t *, int, int))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, len, scanType);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SmapRemoveProcessTrackingTest)
{
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int flag = 1;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_process_tracking_remove(pidArr, len, flag);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapRemoveProcessNullPidaddr)
{
    int ret;
    pid_t *pidArr = nullptr;
    int len = 3;
    int flag = 1;

    ret = ubturbo_smap_process_tracking_remove(pidArr, len, flag);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapRemoveProcessErrLen)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = -1;
    int flag = 1;

    ret = ubturbo_smap_process_tracking_remove(pidArr, len, flag);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapRemoveProcessEncodeRequestFailed)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int flag = 1;

    MOCKER_CPP(&SmapRemoveProcessTrackingCodec::EncodeRequest, int(*)(SmapRemoveProcessTrackingCodec*,
        TurboByteBuffer &, pid_t *, int, int))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_process_tracking_remove(pidArr, len, flag);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapRemoveProcessUBTurboFunctionCallerFailed)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int flag = 1;

    MOCKER_CPP(&SmapRemoveProcessTrackingCodec::EncodeRequest, int(*)(SmapRemoveProcessTrackingCodec*,
        TurboByteBuffer &, pid_t *, int, int))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_process_tracking_remove(pidArr, len, flag);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SmapEnableProcessMigrateTest)
{
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int enable = 1;
    int flags = 1;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_process_migrate_enable(pidArr, len, enable, flags);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapEnableProcessNullPidaddr)
{
    int ret;
    pid_t *pidArr = nullptr;
    int len = 3;
    int enable = 1;
    int flags = 1;

    ret = ubturbo_smap_process_migrate_enable(pidArr, len, enable, flags);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapEnableProcessErrLen)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = -1;
    int enable = 1;
    int flags = 1;

    ret = ubturbo_smap_process_migrate_enable(pidArr, len, enable, flags);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapEnableProcessEncodeRequestFailed)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int enable = 1;
    int flags = 1;

    MOCKER_CPP(&SmapEnableProcessMigrateCodec::EncodeRequest, int(*)(SmapEnableProcessMigrateCodec*, TurboByteBuffer &,
        pid_t *, int, int, int))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_process_migrate_enable(pidArr, len, enable, flags);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapEnableProcessUBTurboFunctionCallerFailed)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int enable = 1;
    int flags = 1;

    MOCKER_CPP(&SmapEnableProcessMigrateCodec::EncodeRequest, int(*)(SmapEnableProcessMigrateCodec*, TurboByteBuffer &,
        pid_t *, int, int, int))
        .stubs()
        .will(returnValue(0));
    
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_process_migrate_enable(pidArr, len, enable, flags);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SetSmapRemoteNumaInfoTest)
{
    struct SetRemoteNumaInfoMsg msg;
    msg.destNid = 4;
    msg.srcNid = 0;
    msg.size = 100;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SetSmapRemoteNullMsg)
{
    int ret;
    struct SetRemoteNumaInfoMsg *msg = nullptr;

    ret = ubturbo_smap_remote_numa_info_set(msg);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SetSmapRemoteEncodeRequestError)
{
    int ret;
    struct SetRemoteNumaInfoMsg msg;
    msg.destNid = 4;
    msg.srcNid = 0;
    msg.size = 100;

    MOCKER_CPP(&SetSmapRemoteNumaInfoCodec::EncodeRequest, int(*)(SetSmapRemoteNumaInfoCodec*, TurboByteBuffer &,
        SetRemoteNumaInfoMsg *))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SetSmapRemoteUBTurboFunctionCallerFailed)
{
    int ret;
    struct SetRemoteNumaInfoMsg msg;
    msg.destNid = 4;
    msg.srcNid = 0;
    msg.size = 100;

    MOCKER_CPP(&SetSmapRemoteNumaInfoCodec::EncodeRequest, int(*)(SetSmapRemoteNumaInfoCodec*, TurboByteBuffer &,
        SetRemoteNumaInfoMsg *))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_remote_numa_info_set(&msg);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SmapQueryVmFreqTest)
{
    int pid = 1;
    uint16_t lengthIn = 10;
    uint16_t lengthOut = 0;
    uint16_t data[lengthIn] = {0};

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_vm_freq_query(pid, data, lengthIn, &lengthOut, 0);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapQueryVmFreqNullData)
{
    int ret;
    int pid = 1;
    uint16_t lengthIn = 10;
    uint16_t lengthOut = 0;
    uint16_t *data = nullptr;

    ret = ubturbo_smap_vm_freq_query(pid, data, lengthIn, &lengthOut, 0);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapQueryVmFreqEmptyLengthIn)
{
    int ret;
    int pid = 1;
    uint16_t lengthIn = 0;
    uint16_t lengthOut = 0;
    uint16_t data[lengthIn] = {0};

    ret = ubturbo_smap_vm_freq_query(pid, data, lengthIn, &lengthOut, 0);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapQueryVmFreqEncodeRequestError)
{
    int ret;
    int pid = 1;
    uint16_t lengthIn = 10;
    uint16_t lengthOut = 0;
    uint16_t data[lengthIn] = {0};

    MOCKER_CPP(&SmapQueryVmFreqCodec::EncodeRequest, int(*)(SmapQueryVmFreqCodec*,
        TurboByteBuffer &, int, uint16_t))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_vm_freq_query(pid, data, lengthIn, &lengthOut, 0);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapQueryVmFreqUBTurboFunctionCallerFailed)
{
    int ret;
    int pid = 1;
    uint16_t lengthIn = 10;
    uint16_t lengthOut = 0;
    uint16_t data[lengthIn] = {0};

    MOCKER_CPP(&SmapQueryVmFreqCodec::EncodeRequest, int(*)(SmapQueryVmFreqCodec*,
        TurboByteBuffer &, int, uint16_t))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_vm_freq_query(pid, data, lengthIn, &lengthOut, 0);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SetSmapRunModeTest)
{
    int runMode = 1;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SetSmapRunModeEncodeRequestError)
{
    int ret;
    int runMode = 1;

    MOCKER_CPP(&SetSmapRunModeCodec::EncodeRequest, int(*)(SetSmapRunModeCodec*, TurboByteBuffer &, int))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SetSmapRunModeUBTurboFunctionCallerFailed)
{
    int ret;
    int runMode = 1;

    MOCKER_CPP(&SetSmapRunModeCodec::EncodeRequest, int(*)(SetSmapRunModeCodec*, TurboByteBuffer &, int))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_run_mode_set(runMode);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SmapIsRunningTest)
{
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    bool ret = ubturbo_smap_is_running();
}

TEST_F(TestSmapClient, SmapIsRunningEncodeRequestError)
{
    bool ret;
    int runMode = 1;

    MOCKER_CPP(&SmapIsRunningCodec::EncodeRequest, int(*)(SmapIsRunningCodec*, TurboByteBuffer&))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_is_running();
    EXPECT_EQ(false, ret);
}

TEST_F(TestSmapClient, SmapIsRunningUBTurboFunctionCallerFailed)
{
    bool ret;
    int runMode = 1;

    MOCKER_CPP(&SmapIsRunningCodec::EncodeRequest, int(*)(SmapIsRunningCodec*, TurboByteBuffer&))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_is_running();
    EXPECT_EQ(false, ret);
}

TEST_F(TestSmapClient, SmapMigrateOutSyncTest)
{
    struct MigrateOutMsg msg = { 0 };
    msg.count = 1;
    msg.payload[0].pid = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].ratio = 25;
    int pidType = 1;
    uint64_t maxWaitTime = 1000;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapMigrateOutSyncNullMsg)
{
    int ret;
    struct MigrateOutMsg *msg = nullptr;
    int pidType = 1;
    uint64_t maxWaitTime = 1000;

    ret = ubturbo_smap_migrate_out_sync(msg, pidType, maxWaitTime);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapMigrateOutSyncEncodeRequestError)
{
    int ret;
    struct MigrateOutMsg msg = { 0 };
    msg.count = 1;
    msg.payload[0].pid = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].ratio = 25;
    int pidType = 1;
    uint64_t maxWaitTime = 1000;

    MOCKER_CPP(&SmapMigrateOutSyncCodec::EncodeRequest, int(*)(SmapMigrateOutSyncCodec*, TurboByteBuffer &,
        MigrateOutMsg *, int, uint64_t))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapMigrateOutSyncUBTurboFunctionCallerFailed)
{
    int ret;
    struct MigrateOutMsg msg = { 0 };
    msg.count = 1;
    msg.payload[0].pid = 1;
    msg.payload[0].destNid = 4;
    msg.payload[0].ratio = 25;
    int pidType = 1;
    uint64_t maxWaitTime = 1000;

    MOCKER_CPP(&SmapMigrateOutSyncCodec::EncodeRequest, int(*)(SmapMigrateOutSyncCodec*, TurboByteBuffer &,
        MigrateOutMsg *, int, uint64_t))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_migrate_out_sync(&msg, pidType, maxWaitTime);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SmapMigrateRemoteNumaTest)
{
    struct MigrateNumaMsg msg = { 0 };

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapMigrateRemoteNumaNullMsg)
{
    int ret;
    struct MigrateNumaMsg *msg = nullptr;

    ret = ubturbo_smap_remote_numa_migrate(msg);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapMigrateRemoteNumaEncodeRequestError)
{
    int ret;
    struct MigrateNumaMsg msg = { 0 };

    MOCKER_CPP(&SmapMigrateRemoteNumaCodec::EncodeRequest, int(*)(SmapMigrateRemoteNumaCodec*, TurboByteBuffer &,
        MigrateNumaMsg *))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapMigrateRemoteNumaUBTurboFunctionCallerFailed)
{
    int ret;
    struct MigrateNumaMsg msg = { 0 };

    MOCKER_CPP(&SmapMigrateRemoteNumaCodec::EncodeRequest, int(*)(SmapMigrateRemoteNumaCodec*, TurboByteBuffer &,
        MigrateNumaMsg *))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_remote_numa_migrate(&msg);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SmapMigratePidRemoteNumaTest)
{
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int srcNid = 0;
    int destNid = 1;

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, len, srcNid, destNid);
    EXPECT_NE(ret, 0);
}

TEST_F(TestSmapClient, SmapMigratePidRemoteNumaNullMsg)
{
    int ret;
    pid_t *pidArr = nullptr;
    int len = 3;
    int srcNid = 0;
    int destNid = 1;

    ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, len, srcNid, destNid);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapMigratePidRemoteNumaErrLen)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = -1;
    int srcNid = 0;
    int destNid = 1;

    ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, len, srcNid, destNid);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapMigratePidRemoteNumaEncodeRequestError)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int srcNid = 0;
    int destNid = 1;

    MOCKER_CPP(&SmapMigratePidRemoteNumaCodec::EncodeRequest, int(*)(SmapMigratePidRemoteNumaCodec*, TurboByteBuffer &,
        pid_t *, int, int, int))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, len, srcNid, destNid);
    EXPECT_EQ(1, ret);
}

TEST_F(TestSmapClient, SmapMigratePidRemoteNumaUBTurboFunctionCallerFailed)
{
    int ret;
    pid_t pidArr[] = {1, 2, 3};
    int len = 3;
    int srcNid = 0;
    int destNid = 1;

    MOCKER_CPP(&SmapMigratePidRemoteNumaCodec::EncodeRequest, int(*)(SmapMigratePidRemoteNumaCodec*, TurboByteBuffer &,
        pid_t *, int, int, int))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(2));

    ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, len, srcNid, destNid);
    EXPECT_EQ(2, ret);
}

TEST_F(TestSmapClient, SmapQueryProcessConfigTest)
{
    int nid = 1;
    struct ProcessPayload payload = { 0 };
    int inLen = 1;
    int outLen;
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(invoke(Test_UBTurboFunctionCaller));
    int ret = ubturbo_smap_process_config_query(nid, &payload, inLen, &outLen);
    EXPECT_NE(ret, 0);

    GlobalMockObject::verify();
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(1));
    ret = ubturbo_smap_process_config_query(nid, &payload, inLen, &outLen);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestSmapClient, SmapQueryProcessConfigNullMsg)
{
    int ret;
    int nid = 1;
    struct ProcessPayload *payload = nullptr;
    int inLen = 1;
    int outLen;

    ret = ubturbo_smap_process_config_query(nid, payload, inLen, &outLen);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapQueryProcessConfigErrLen)
{
    int ret;
    int nid = 1;
    struct ProcessPayload payload = { 0 };
    int inLen = -1;
    int outLen;

    ret = ubturbo_smap_process_config_query(nid, &payload, inLen, &outLen);
    EXPECT_EQ(-22, ret);
}

TEST_F(TestSmapClient, SmapQueryProcessConfigEncodeRequestError)
{
    int ret;
    int nid = 1;
    struct ProcessPayload payload = { 0 };
    int inLen = 1;
    int outLen;

    MOCKER_CPP(&SmapQueryProcessConfigCodec::EncodeRequest, int(*)(SmapQueryProcessConfigCodec*,
        TurboByteBuffer &, int, int))
        .stubs()
        .will(returnValue(-1));

    ret = ubturbo_smap_process_config_query(nid, &payload, inLen, &outLen);
    EXPECT_EQ(1, ret);
}

const int FREQ_QUERY = 19;

TEST_F(TestSmapClient, SmapQueryNumaFreqInvalidParameter)
{
    int ret = ubturbo_smap_remote_numa_freq_query(nullptr, nullptr, 1);
    EXPECT_EQ(-EINVAL, ret);

    uint16_t numa;
    uint64_t freq;
    ret = ubturbo_smap_remote_numa_freq_query(&numa, &freq, FREQ_QUERY);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(TestSmapClient, SmapQueryNumaFreq)
{
    uint16_t numa;
    uint64_t freq;

    MOCKER_CPP(&SmapQueryRemoteNumaFreqCodec::EncodeRequest, int(*)(SmapQueryRemoteNumaFreqCodec*,
        TurboByteBuffer &, uint16_t *, uint16_t))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&UBTurboFunctionCaller, uint32_t(*)(const std::string &function, const TurboByteBuffer &params,
        TurboByteBuffer &result))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&SmapQueryRemoteNumaFreqCodec::DecodeResponse, int(*)(SmapQueryRemoteNumaFreqCodec*,
        TurboByteBuffer &, uint16_t *, uint16_t))
        .stubs()
        .will(returnValue(IPC_ERROR));
    int ret = ubturbo_smap_remote_numa_freq_query(&numa, &freq, 1);
    EXPECT_EQ(IPC_ERROR, ret);
}