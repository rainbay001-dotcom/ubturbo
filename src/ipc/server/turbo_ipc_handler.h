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
#ifndef TURBO_IPC_HANDLER_H
#define TURBO_IPC_HANDLER_H

#include <string>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <sys/un.h>
#include <thread>
#include "turbo_def.h"
#include "turbo_common.h"

namespace turbo::ipc::server {
using namespace turbo::common;

class IpcHandler {
public:
    static IpcHandler &Instance();
    RetCode StartListen();
    RetCode EndListen();
    RetCode UBTurboRegIpcService(const std::string &name, IpcHandlerFunc function);
    RetCode UBTurboUnRegIpcService(const std::string &name);
private:
    IpcHandler() = default;
    std::shared_mutex gLock;
    void PThreadListen();
    void PThreadHandle(int fd);
    RetCode HandleFunction(const std::string &functionName, const TurboByteBuffer &messageBuffer, int fd);
    std::unordered_map<std::string, IpcHandlerFunc> funcTable;
    struct sockaddr_un addr{};
    int listenFd{};
    bool running{false};
    std::thread *pThread{nullptr};
};

}

#endif