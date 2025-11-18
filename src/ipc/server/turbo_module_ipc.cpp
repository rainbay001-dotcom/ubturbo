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
#include "turbo_common.h"
#include "turbo_error.h"
#include "turbo_ipc_server_inner.h"

#include "turbo_module_ipc.h"

namespace turbo::ipc::server {

using namespace turbo::common;

RetCode TurboModuleIPC::Init()
{
    return TURBO_OK;
}

RetCode TurboModuleIPC::Start()
{
    return IpcServerInner::Instance().StartListen();
}

void TurboModuleIPC::Stop()
{
    IpcServerInner::Instance().EndListen();
    return ;
}

void TurboModuleIPC::UnInit() {}

std::string TurboModuleIPC::Name()
{
    return "ipc";
}

}  //  namespace turbo::ipc::server