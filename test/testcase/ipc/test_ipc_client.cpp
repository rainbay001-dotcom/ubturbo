#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "securec.h"
#include "turbo_def.h"
#define private public
#include "turbo_ipc_client_inner.h"
#include "turbo_ipc_client.h"
#undef private
#include "ulog.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace turbo {
using namespace turbo::ipc::client;

const std::string UDS_PATH = "/opt/ubturbo";
const __mode_t DIR_MODE = 0600;

class TurboIpcTest : public ::testing::Test {
public:
    IpcClientInner *ipcClient;
    TurboByteBuffer params;
    TurboByteBuffer result;

    void SetUp() override
    {
        mkdir(UDS_PATH.c_str(), DIR_MODE);
        ipcClient = new IpcClientInner();
        params.data = nullptr;
        params.len = 0;
        params.freeFunc = nullptr;
        result.data = nullptr;
        result.len = 0;
        result.freeFunc = nullptr;
        int socket_pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socket_pair) == -1) {
            FAIL() << "Failed to create socket pair";
        }
        client_fd = socket_pair[0];
        server_fd = socket_pair[1];
    }

    void TearDown() override
    {
        delete ipcClient;
        close(client_fd);
        close(server_fd);
        GlobalMockObject::verify();
    }
    int client_fd;
    int server_fd;
};

uint32_t GetHeader(uint8_t *message);

TEST_F(TurboIpcTest, ReturnEqualWhenInvokeGetInstanceTwice)
{
    IpcClientInner &client1 = IpcClientInner::Instance();
    IpcClientInner &client2 = IpcClientInner::Instance();
    EXPECT_TRUE(&client1 == &client2);
}

TEST_F(TurboIpcTest, CheckReturnValueWhenInit)
{
    IpcClientInner client;
    uint32_t ret = client.Init();
    EXPECT_EQ(ret, IPC_OK);
}

TEST_F(TurboIpcTest, ShouldReturnERR_WhenSocketFunctionsReturnERR)
{
    std::string function = "testFunction";
    MOCKER_CPP(socket, int (*)(int __domain, int __type, int __protocol)).stubs().will(returnValue(-1));
    MOCKER_CPP(close, int (*)(int __fd)).stubs().will(returnValue(0));
    uint32_t ret = ipcClient->UBTurboFunctionCaller(function, params, result);
    EXPECT_EQ(ret, IPC_BAD_SOCKET);
}

TEST_F(TurboIpcTest, ShouldReturnERR_WhenMemsetFunctionsReturnERR)
{
    std::string function = "testFunction";
    MOCKER_CPP(socket, int (*)(int __domain, int __type, int __protocol)).stubs().will(returnValue(1));
    MOCKER_CPP(memset_s, errno_t(*)(void *dest, size_t destMax, int c, size_t count)).stubs().will(returnValue(1));
    MOCKER_CPP(close, int (*)(int __fd)).stubs().will(returnValue(0));
    uint32_t ret = ipcClient->UBTurboFunctionCaller(function, params, result);
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, ShouldReturnERR_WhenStrcpyFunctionsReturnERR)
{
    std::string function = "testFunction";
    MOCKER_CPP(close, int (*)(int __fd)).stubs().will(returnValue(0));
    MOCKER_CPP(socket, int (*)(int __domain, int __type, int __protocol)).stubs().will(returnValue(0));
    MOCKER_CPP(memset_s, errno_t(*)(void *dest, size_t destMax, int c, size_t count)).stubs().will(returnValue(0));

    MOCKER_CPP(strcpy_s, errno_t(*)(char *strDest, size_t destMax, const char *strSrc)).stubs().will(returnValue(1));
    uint32_t ret = ipcClient->UBTurboFunctionCaller(function, params, result);
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, ShouldReturnERR_WhenConnectFunctionsReturnERR)
{
    std::string function = "testFunction";
    MOCKER_CPP(close, int (*)(int __fd)).stubs().will(returnValue(0));
    MOCKER_CPP(socket, int (*)(int __domain, int __type, int __protocol)).stubs().will(returnValue(0));
    MOCKER_CPP(memset_s, errno_t(*)(void *dest, size_t destMax, int c, size_t count)).stubs().will(returnValue(0));
    MOCKER_CPP(strcpy_s, errno_t(*)(char *strDest, size_t destMax, const char *strSrc)).stubs().will(returnValue(0));
    MOCKER_CPP(connect, int (*)(int fd, const struct sockaddr *addr, socklen_t len)).stubs().will(returnValue(-1));
    uint32_t ret = ipcClient->UBTurboFunctionCaller(function, params, result);
    EXPECT_EQ(ret, IPC_BAD_CONNECT);
}

TEST_F(TurboIpcTest, ShouldReturnERR_WhenSendMessageFunctionsReturnERR)
{
    std::string function = "testFunction";
    MOCKER_CPP(close, int (*)(int __fd)).stubs().will(returnValue(0));
    MOCKER_CPP(socket, int (*)(int __domain, int __type, int __protocol)).stubs().will(returnValue(0));
    MOCKER_CPP(memset_s, errno_t(*)(void *dest, size_t destMax, int c, size_t count)).stubs().will(returnValue(0));
    MOCKER_CPP(strcpy_s, errno_t(*)(char *strDest, size_t destMax, const char *strSrc)).stubs().will(returnValue(0));
    MOCKER_CPP(connect, int (*)(int fd, const struct sockaddr *addr, socklen_t len)).stubs().will(returnValue(0));
    MOCKER_CPP(&IpcClientInner::SendMessage,
               uint32_t(*)(int fd, const std::string &function, const TurboByteBuffer &params))
        .stubs()
        .will(returnValue(IPC_ERROR));
    uint32_t ret = ipcClient->UBTurboFunctionCaller(function, params, result);
    EXPECT_EQ(ret, IPC_BAD_CONNECT);
}

TEST_F(TurboIpcTest, ShouldReturnERR_WhenRecvMessageFunctionsReturnERR)
{
    std::string function = "testFunction";
    MOCKER_CPP(close, int (*)(int __fd)).stubs().will(returnValue(0));
    MOCKER_CPP(socket, int (*)(int __domain, int __type, int __protocol)).stubs().will(returnValue(0));
    MOCKER_CPP(memset_s, errno_t(*)(void *dest, size_t destMax, int c, size_t count)).stubs().will(returnValue(0));
    MOCKER_CPP(strcpy_s, errno_t(*)(char *strDest, size_t destMax, const char *strSrc)).stubs().will(returnValue(0));
    MOCKER_CPP(connect, int (*)(int fd, const struct sockaddr *addr, socklen_t len)).stubs().will(returnValue(0));
    MOCKER_CPP(&IpcClientInner::SendMessage,
               uint32_t(*)(int fd, const std::string &function, const TurboByteBuffer &params))
        .stubs()
        .will(returnValue(IPC_OK));
    MOCKER_CPP(&IpcClientInner::RecvMessage, uint32_t(*)(int fd, TurboByteBuffer &result))
        .stubs()
        .will(returnValue(IPC_ERROR));
    uint32_t ret = ipcClient->UBTurboFunctionCaller(function, params, result);
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, ShouldReturnOKWhenALLPASSReturnOK)
{
    std::string function = "testFunction";
    MOCKER_CPP(close, int (*)(int __fd)).stubs().will(returnValue(0));
    MOCKER_CPP(socket, int (*)(int __domain, int __type, int __protocol)).stubs().will(returnValue(1));
    MOCKER_CPP(memset_s, errno_t(*)(void *dest, size_t destMax, int c, size_t count)).stubs().will(returnValue(0));
    MOCKER_CPP(strcpy_s, errno_t(*)(char *strDest, size_t destMax, const char *strSrc)).stubs().will(returnValue(0));
    MOCKER_CPP(connect, int (*)(int fd, const struct sockaddr *addr, socklen_t len)).stubs().will(returnValue(0));
    MOCKER_CPP(&IpcClientInner::SendMessage,
               uint32_t(*)(int fd, const std::string &function, const TurboByteBuffer &params))
        .stubs()
        .will(returnValue(IPC_OK));
    MOCKER_CPP(&IpcClientInner::RecvMessage, uint32_t(*)(int fd, TurboByteBuffer &result))
        .stubs()
        .will(returnValue(IPC_OK));
    uint32_t ret = ipcClient->UBTurboFunctionCaller(function, params, result);
    EXPECT_EQ(ret, IPC_OK);
}
TEST_F(TurboIpcTest, RecvMessageShouldReturnERRWhensetsockoptReturnERR)
{
    MOCKER_CPP(setsockopt, int (*)(int __fd, int __level, int __optname, const void *__optva))
        .stubs()
        .will(returnValue(-1));
    uint32_t ret = ipcClient->RecvMessage(0, result);
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, RecvMessageShouldReturnERRWhenreceivedLengthNegative)
{
    MOCKER_CPP(setsockopt, int (*)(int __fd, int __level, int __optname, const void *__optva))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(recv, ssize_t(*)(int __fd, void *__buf, size_t __len, int __flags)).stubs().will(returnValue(-1));
    uint32_t ret = ipcClient->RecvMessage(0, result);
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, RecvMessageShouldReturnERRWhenreceivedLengthLessThanMIN_MESSAGE_LENGTH)
{
    MOCKER_CPP(setsockopt, int (*)(int __fd, int __level, int __optname, const void *__optva))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(recv, ssize_t(*)(int __fd, void *__buf, size_t __len, int __flags)).stubs().will(returnValue(3));
    uint32_t ret = ipcClient->RecvMessage(0, result);
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, RecvMessageShouldReturnERRWhenreceivedLengthMoreThanMAXLEN)
{
    MOCKER_CPP(setsockopt, int (*)(int __fd, int __level, int __optname, const void *__optva))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(recv, ssize_t(*)(int __fd, void *__buf, size_t __len, int __flags))
        .stubs()
        .will(returnValue(1024 * 1024 + 1));
    uint32_t ret = ipcClient->RecvMessage(0, result);
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, SendMessageReturnErrWhenMemcpyErr)
{
    MOCKER_CPP(memcpy_s, errno_t(*)(void *dest, size_t destMax, const void *src, size_t count))
        .stubs()
        .will(returnValue(1));
    TurboByteBuffer params;
    params.data = (uint8_t*)"testParams";
    params.len = strlen((const char*)params.data);
    uint32_t ret = ipcClient->SendMessage(0, "test", params);
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, SendMessageReturnErrWhenSendErr)
{
    MOCKER_CPP(memcpy_s, errno_t(*)(void *dest, size_t destMax, const void *src, size_t count))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(send, errno_t(*)(int sockfd, const void *buf, size_t len, int flags))
        .stubs()
        .will(returnValue(0));
    uint32_t ret = ipcClient->SendMessage(0, "test", TurboByteBuffer());
    EXPECT_EQ(ret, IPC_ERROR);
}

TEST_F(TurboIpcTest, NormalSendMessage)
{
    const std::string function = "testFunction";
    TurboByteBuffer params;
    params.data = (uint8_t*)"testParams";
    params.len = strlen((const char*)params.data);

    // 调用SendMessage
    uint32_t result = ipcClient->SendMessage(client_fd, function, params);

    // 验证结果
    EXPECT_EQ(result, IPC_OK);
}

TEST_F(TurboIpcTest, NormalRecvMessage)
{
    // 准备发送的数据
    std::string function = "function";
    TurboByteBuffer send_params;
    send_params.data = (uint8_t*)"testParams";
    send_params.len = strlen((const char*)send_params.data);

    // 发送消息
    uint32_t send_result = ipcClient->SendMessage(client_fd, function, send_params);
    EXPECT_EQ(send_result, IPC_OK);

    // 接收消息
    TurboByteBuffer recv_result;
    uint32_t recv_result_code = ipcClient->RecvMessage(server_fd, recv_result);
    EXPECT_EQ(recv_result_code, function.length() + 1);
    EXPECT_EQ(recv_result.len, 0);
    // 释放接收的数据
    delete[] recv_result.data;
}

TEST_F(TurboIpcTest, TestUBTurboFunctionCaller)
{
    std::string function = "testFunction";
    UBTurboFunctionCaller(function, params, result);
}

TEST_F(TurboIpcTest, SendMessageErrWhenMemcpyErr)
{
    // 准备发送的数据
    std::string function = "testFunction";
    TurboByteBuffer send_params;
    send_params.data = (uint8_t*)"testParams";
    send_params.len = strlen((const char*)send_params.data);
    MOCKER_CPP(memcpy_s, errno_t(*)(void *dest, size_t destMax, const void *src, size_t count))
        .stubs()
        .will(returnValue(1));
    // 发送消息
    uint32_t send_result = ipcClient->SendMessage(client_fd, function, send_params);
    EXPECT_EQ(send_result, IPC_ERROR);
    // 释放接收的数据
}

TEST_F(TurboIpcTest, UBTurboFunctionCallerFailed1)
{
    MOCKER_CPP(&IpcClientInner::CheckInputIsValid, bool(*)(IpcClientInner *, const std::string &,
        const TurboByteBuffer &))
        .stubs()
        .will(returnValue(false));
    TurboByteBuffer params;
    TurboByteBuffer result;
    uint32_t ret = IpcClientInner::Instance().UBTurboFunctionCaller("name", params, result);
    EXPECT_EQ(ret, IPC_ERROR);
}

const int MAX_LENGTH = 128;

TEST_F(TurboIpcTest, CheckInputIsValid1)
{
    TurboByteBuffer buffer;
    bool ret = IpcClientInner::Instance().CheckInputIsValid(" ", buffer);
    EXPECT_EQ(ret, false);
}

TEST_F(TurboIpcTest, CheckInputIsValid2)
{
    std::string function;
    function.resize(MAX_LENGTH + 1);
    TurboByteBuffer buffer;
    bool ret = IpcClientInner::Instance().CheckInputIsValid(function, buffer);
    EXPECT_EQ(ret, false);
}

static const size_t BUFFER_MAX_LEN = 1024 * 1024;

TEST_F(TurboIpcTest, CheckInputIsValid3)
{
    TurboByteBuffer buffer;
    buffer.len = BUFFER_MAX_LEN + 1;
    bool ret = IpcClientInner::Instance().CheckInputIsValid("name", buffer);
    EXPECT_EQ(ret, false);
}

TEST_F(TurboIpcTest, CheckInputIsValid4)
{
    TurboByteBuffer buffer;
    buffer.len = 1;
    bool ret = IpcClientInner::Instance().CheckInputIsValid("name", buffer);
    EXPECT_EQ(ret, false);
}

TEST_F(TurboIpcTest, CheckInputIsSucceed)
{
    TurboByteBuffer buffer;
    bool ret = IpcClientInner::Instance().CheckInputIsValid("name", buffer);
    EXPECT_EQ(ret, true);
}

TEST_F(TurboIpcTest, GetHeaderSucceed)
{
    uint8_t *message = new uint8_t[sizeof(uint32_t)];
    message[0] = 1;
    auto ret = IpcClientInner::GetHeader(message);
    EXPECT_NE(ret, 0);
    delete[] message;
}

TEST_F(TurboIpcTest, GetHeaderFailed)
{
    MOCKER_CPP(memcpy_s, errno_t(*)(void *dest, size_t destMax, const void *src, size_t count))
        .stubs()
        .will(returnValue(1));
    uint8_t *message = new uint8_t[sizeof(uint32_t)];
    message[0] = 1;
    auto ret = IpcClientInner::GetHeader(message);
    EXPECT_EQ(ret, 0);
    delete[] message;
}

TEST_F(TurboIpcTest, SetHeaderSucceed)
{
    uint32_t number = 1234;
    uint8_t before[sizeof(uint32_t)] = {0};
    uint8_t after[sizeof(uint32_t)] = {0};

    IpcClientInner::SetHeader(after, number);
    EXPECT_NE(memcmp(before, after, sizeof(uint32_t)), 0);
}

TEST_F(TurboIpcTest, SetHeaderFailed)
{
    MOCKER_CPP(memcpy_s, errno_t(*)(void *dest, size_t destMax, const void *src, size_t count))
        .stubs()
        .will(returnValue(1));
    uint8_t *message = new uint8_t[sizeof(uint32_t)];
    message[0] = 1;
    memset_s(message, sizeof(uint32_t), 0, sizeof(uint32_t));
    uint32_t number = 0;
    IpcClientInner::SetHeader(message, number);
    EXPECT_EQ(message[0], 0);
    delete[] message;
}

} // namespace turbo