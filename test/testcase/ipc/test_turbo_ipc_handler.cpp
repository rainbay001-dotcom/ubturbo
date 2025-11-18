/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.‘
 */

#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <arpa/inet.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include "turbo_error.h"
#include "turbo_ipc_handler.h"
#include "turbo_ipc_server_inner.h"
#include "turbo_def.h"
#include "turbo_ipc_client_inner.h"
#include "turbo_ipc_client.h"
#include "turbo_ipc_server.h"
#include "securec.h"
#include "turbo_module_ipc.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace turbo::ipc::server;
using namespace turbo::ipc::client;

namespace turbo::ipc_server {

// 测试类
class TestTurboIpcHandler : public ::testing::Test {
public:
    IpcClientInner *ipcClient;
    IpcServerInner *ipcServer;
    TurboModuleIPC *module;
    TurboByteBuffer params;
    TurboByteBuffer result;
    IpcHandler *ipcHandler;
    int listenFd;
    void SetUp() override
    {
        ipcClient = new IpcClientInner();
        ipcServer = new IpcServerInner();
        module = new TurboModuleIPC();
        params.data = nullptr;
        params.len = 0;
        params.freeFunc = nullptr;
        result.data = nullptr;
        result.len = 0;
        result.freeFunc = nullptr;
        int socketPair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketPair) == -1) {
            FAIL() << "Failed to create socket pair";
        }
        clientFd = socketPair[0];
        serverFd = socketPair[1];
        ipcHandler = new IpcHandler();
        listenFd = serverFd;
        ipcHandler->listenFd = listenFd;
    }

    void TearDown() override
    {
        delete ipcClient;
        delete ipcHandler;
        close(clientFd);
        close(serverFd);
        GlobalMockObject::verify();
    }
    int clientFd;
    int serverFd;
};

uint32_t IpcServiceFuncForTest(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    return 0;
}

TEST_F(TestTurboIpcHandler, UBTurboRegIpcServiceSucceed1)
{
    const std::string function = "testFunction1";
    TurboByteBuffer params;
    params.data = (uint8_t *)"testParams";
    params.len = strlen((const char *)params.data);
    auto res = IpcHandler::Instance().UBTurboRegIpcService(function, IpcServiceFuncForTest);
    EXPECT_EQ(res, TURBO_OK);
}

TEST_F(TestTurboIpcHandler, UBTurboRegIpcServiceFailed1)
{
    const std::string function = "testFunction1";
    TurboByteBuffer params;
    params.data = (uint8_t *)"testParams";
    params.len = strlen((const char *)params.data);
    std::function<uint32_t(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)> emptyFunction;
    auto res = IpcHandler::Instance().UBTurboRegIpcService(function, emptyFunction);
    EXPECT_EQ(res, TURBO_ERROR);
}

const int MAX_LENGTH = 128;

TEST_F(TestTurboIpcHandler, UBTurboRegIpcServiceFailed2)
{
    std::string function = "";
    function.resize(MAX_LENGTH + 1);
    TurboByteBuffer params;
    params.data = (uint8_t *)"testParams";
    params.len = strlen((const char *)params.data);
    std::function<uint32_t(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)> emptyFunction;
    auto res = IpcHandler::Instance().UBTurboRegIpcService(function, emptyFunction);
    EXPECT_EQ(res, TURBO_ERROR);
}

TEST_F(TestTurboIpcHandler, UBTurboUnRegIpcServiceSucceed)
{
    const std::string function = "testFunction1";
    TurboByteBuffer params;
    params.data = (uint8_t *)"testParams";
    params.len = strlen((const char *)params.data);
    uint32_t result = ipcClient->SendMessage(clientFd, function, params);
    auto res = IpcHandler::Instance().UBTurboUnRegIpcService(function);
    EXPECT_EQ(res, TURBO_OK);
}

TEST_F(TestTurboIpcHandler, StartListenSucceed)
{
    auto res = IpcHandler::Instance().StartListen();
    EXPECT_EQ(res, TURBO_OK);
}

TEST_F(TestTurboIpcHandler, EndListenSucceed)
{
    auto res = IpcHandler::Instance().EndListen();
    EXPECT_EQ(res, TURBO_OK);
}

TEST_F(TestTurboIpcHandler, PThreadListenSucceed)
{
    const std::string function = "testFunction3";
    TurboByteBuffer params;
    params.data = (uint8_t *)"testParams";
    params.len = strlen((const char *)params.data);
    uint32_t result = ipcClient->SendMessage(clientFd, function, params);
    IpcHandlerFunc func;
    IpcHandler::Instance().PThreadListen();
}

TEST_F(TestTurboIpcHandler, PThreadHandleFailed1)
{
    int fd = 0;
    IpcHandler::Instance().PThreadHandle(fd);
}

TEST_F(TestTurboIpcHandler, PThreadHandleFailed2)
{
    const std::string function = "testFunction-1";
    TurboByteBuffer params;
    params.data = (uint8_t *)"testParams";
    params.len = strlen((const char *)params.data);
    uint32_t result = ipcClient->SendMessage(clientFd, function, params);
    IpcHandler::Instance().PThreadHandle(serverFd);
}

TEST_F(TestTurboIpcHandler, ShouldLogSelectError_WhenSelectReturnsNegative)
{
    MOCKER_CPP(select, int (*)(int, fd_set *, fd_set *, fd_set *, struct timeval *)).stubs().will(returnValue(-1));
    ipcHandler->PThreadListen();
}

TEST_F(TestTurboIpcHandler, ShouldContinue_WhenSelectReturnsZero)
{
    MOCKER_CPP(select, int (*)(int, fd_set *, fd_set *, fd_set *, struct timeval *)).stubs().will(returnValue(0));
    ipcHandler->PThreadListen();
}

uint32_t TestFunc(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    outputBuffer.data = new uint8_t[3];
    outputBuffer.data[0] = 'b';
    outputBuffer.data[1] = 'y';
    outputBuffer.data[2] = 'e';
    outputBuffer.len = 3;
    return 0;
}

uint32_t TestFuncFailed(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    return 1;
}

static const int LENGTH_OUT_LIMIT = 1024 * 1024;

uint32_t TestFuncTooLong(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    outputBuffer.len = LENGTH_OUT_LIMIT;
    return 0;
}

TEST_F(TestTurboIpcHandler, ListenSuccess)
{
    UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
    params.data = new uint8_t[5];
    params.len = 5;
    params.data[0] = 'h';
    params.data[1] = 'e';
    params.data[2] = 'l';
    params.data[3] = 'l';
    params.data[4] = 'o';
    UBTurboFunctionCaller("function", params, result);
    ipcHandler->EndListen();
}

TEST_F(TestTurboIpcHandler, ListenUnSuccess)
{
    UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
    params.data = new uint8_t[5];
    params.len = 5;
    params.data[0] = 'h';
    params.data[1] = 'e';
    params.data[2] = 'l';
    params.data[3] = 'l';
    params.data[4] = 'o';
    UBTurboFunctionCaller("function2", params, result);
    ipcHandler->EndListen();
}

TEST_F(TestTurboIpcHandler, TestHandleFunctionFuncExits)
{
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    int fd = 0;
    ipcHandler->UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
    ipcHandler->HandleFunction("function", messageBuffer, 0);
    ipcHandler->EndListen();
    // Assert
}

TEST_F(TestTurboIpcHandler, TestHandleFunctionFuncExitsReturnERRWhenFuntionFailed)
{
    // Arrang
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    ipcHandler->UBTurboRegIpcService("function", TestFuncFailed);
    ipcHandler->StartListen();
    ipcHandler->HandleFunction("function", messageBuffer, 0);
    ipcHandler->EndListen();
    // Assert
}

TEST_F(TestTurboIpcHandler, TestHandleFunctionFuncExitsReturnERRWhenFuntionTooLong)
{
    // Arrang
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    ipcHandler->UBTurboRegIpcService("function", TestFuncTooLong);
    ipcHandler->StartListen();
    ipcHandler->HandleFunction("function", messageBuffer, 0);
    ipcHandler->EndListen();
    // Assert
}

TEST_F(TestTurboIpcHandler, TestHandleFunctionFuncExitsReturnERRWhenMEMCPYERR)
{
    // Arrang
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    MOCKER_CPP(memcpy_s, errno_t(*)(void *dest, size_t destMax, const void *src, size_t count))
        .stubs()
        .will(returnValue(1));

    int fd = 0; // 假设fd已经有效
    // Act
    ipcHandler->UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
    ipcHandler->HandleFunction("function", messageBuffer, 0);
    ipcHandler->EndListen();
    // Assert
}

TEST_F(TestTurboIpcHandler, TestHandleFunctionFuncExitsReturnERRWhenSendERR)
{
    // Arrang
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    MOCKER_CPP(send, ssize_t(*)(int sockfd, const void *buf, size_t len, int flags)).stubs().will(returnValue(0));
    int fd = 0; // 假设fd已经有效
    // Act
    ipcHandler->UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
    ipcHandler->HandleFunction("function", messageBuffer, 0);
    ipcHandler->EndListen();
    // Assert
}

TEST_F(TestTurboIpcHandler, TestStartListenErrWhenSokcetErr)
{
    // Arrang
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    MOCKER_CPP(socket, int (*)(int __domain, int __type, int __protocol)).stubs().will(returnValue(-1));
    // Act
    ipcHandler->UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
}

TEST_F(TestTurboIpcHandler, TestStartListenErrWhenstrcpyErr)
{
    // Arrang
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    MOCKER_CPP(strcpy_s, errno_t(*)(char *dest, size_t destMax, const char *src)).stubs().will(returnValue(-1));
    // Act
    ipcHandler->UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
}

TEST_F(TestTurboIpcHandler, TestStartListenErrWhenlistenErr)
{
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    MOCKER_CPP(listen, int (*)(int sockfd, int num)).stubs().will(returnValue(-1));
    ipcHandler->UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
}

TEST_F(TestTurboIpcHandler, TestStartListenErrWhenmemsetErr)
{
    TurboByteBuffer messageBuffer;
    messageBuffer.data = (uint8_t *)"testFunction testData";
    messageBuffer.len = 16;
    MOCKER_CPP(memset_s, errno_t(*)(void *dest, size_t destMax, int c, size_t count)).stubs().will(returnValue(-1));
    ipcHandler->UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
}

TEST_F(TestTurboIpcHandler, TestStartListenErrWhenbindErr)
{
    MOCKER_CPP(bind, errno_t(*)(int fd, const struct sockaddr *addr, socklen_t len)).stubs().will(returnValue(1));
    auto ret = ipcHandler->StartListen();
    EXPECT_EQ(ret, 1);
}

TEST_F(TestTurboIpcHandler, TestStartListenErrWhenchmodErr)
{
    MOCKER_CPP(chmod, errno_t(*)(const char *file, __mode_t mode)).stubs().will(returnValue(1));
    auto ret = ipcHandler->StartListen();
    EXPECT_EQ(ret, 1);
}

TEST_F(TestTurboIpcHandler, ListenSuccessUnListenFail)
{
    UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
    params.data = new uint8_t[5];
    params.len = 5;
    params.data[0] = 'h';
    params.data[1] = 'e';
    params.data[2] = 'l';
    params.data[3] = 'l';
    params.data[4] = 'o';
    UBTurboFunctionCaller("function", params, result);
    ipcHandler->EndListen();
    ipcHandler->EndListen();
}

TEST_F(TestTurboIpcHandler, DoubleListenSuccessUnListenFail)
{
    UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
    ipcHandler->StartListen();
    params.data = new uint8_t[5];
    params.len = 5;
    params.data[0] = 'h';
    params.data[1] = 'e';
    params.data[2] = 'l';
    params.data[3] = 'l';
    params.data[4] = 'o';
    UBTurboFunctionCaller("function", params, result);
    ipcHandler->EndListen();
    ipcHandler->EndListen();
}

TEST_F(TestTurboIpcHandler, UBTurboUnRegIpcServiceFail)
{
    ipcHandler->UBTurboUnRegIpcService("function3");
}
TEST_F(TestTurboIpcHandler, UBTurboUnRegIpcService)
{
    UBTurboUnRegIpcService("function3");
}

TEST_F(TestTurboIpcHandler, TESTIpcServerInnerStartListen)
{
    ipcServer->StartListen();
}

TEST_F(TestTurboIpcHandler, TESTIpcServerInnerEndListen)
{
    ipcServer->EndListen();
}

TEST_F(TestTurboIpcHandler, PThreadFailedWhenHandleFunctionFailed)
{
    MOCKER_CPP(&IpcHandler::HandleFunction, uint32_t(*)(const std::string &, const TurboByteBuffer &, int))
        .stubs().will(returnValue(TURBO_ERROR));
    UBTurboRegIpcService("function", TestFunc);
    ipcHandler->StartListen();
    params.data = new uint8_t[5];
    params.len = 5;
    params.data[0] = 'h';
    params.data[1] = 'e';
    params.data[2] = 'l';
    params.data[3] = 'l';
    params.data[4] = 'o';
    UBTurboFunctionCaller("function", params, result);
    ipcHandler->EndListen();
}

}