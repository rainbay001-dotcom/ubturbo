/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "turbo_ipc_server_inner.h"

#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>

#include "turbo_logger.h"
#include "turbo_ipc_handler.h"

namespace turbo::ipc::server {

IpcServerInner& IpcServerInner::Instance()
{
    static IpcServerInner instance;
    return instance;
}

RetCode IpcServerInner::UBTurboRegIpcService(const std::string &name, IpcHandlerFunc function)
{
    return IpcHandler::Instance().UBTurboRegIpcService(name, function);
}

RetCode IpcServerInner::UBTurboUnRegIpcService(const std::string &name)
{
    return IpcHandler::Instance().UBTurboUnRegIpcService(name);
}

RetCode IpcServerInner::StartListen()
{
    return IpcHandler::Instance().StartListen();
}

RetCode IpcServerInner::EndListen()
{
    return IpcHandler::Instance().EndListen();
}

}