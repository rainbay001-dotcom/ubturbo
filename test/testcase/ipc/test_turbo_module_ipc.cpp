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
#include "turbo_module_ipc.h"

#define private public
#include "turbo_error.h"
#include "turbo_ipc_handler.h"
#include "turbo_ipc_server_inner.h"
#include "turbo_def.h"
#include "turbo_ipc_client_inner.h"
#include "turbo_ipc_client.h"
#include "turbo_ipc_server.h"
#include "securec.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

using namespace std;
using namespace turbo::ipc::server;
using namespace turbo::ipc::client;

namespace turbo::ipc_server {

// 测试类
class TestTurboIpcModule : public ::testing::Test {
public:
    TurboModuleIPC *module;
    void SetUp() override
    {
        module = new TurboModuleIPC();
    }
    void TearDown() override
    {
        delete module;
        GlobalMockObject::verify();
    }
};
TEST_F(TestTurboIpcModule, TESTModuleInit)
{
    module->Init();
}
TEST_F(TestTurboIpcModule, TESTModuleUnInit)
{
    module->UnInit();
}
TEST_F(TestTurboIpcModule, TESTModuleStart)
{
    module->Start();
}
TEST_F(TestTurboIpcModule, TESTModuleStop)
{
    module->Stop();
}

TEST_F(TestTurboIpcModule, TESTModuleName)
{
    module->Name();
}

}