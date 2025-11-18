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
#include "turbo_ipc_client.h"
#include "turbo_ipc_client_inner.h"

namespace turbo::ipc::client {

uint32_t UBTurboFunctionCaller(const std::string &function, const TurboByteBuffer &params, TurboByteBuffer &result)
{
    return IpcClientInner::Instance().UBTurboFunctionCaller(function, params, result);
}

uint32_t SetIpcTimeLimit(uint32_t timeLimit)
{
    return IpcClientInner::Instance().SetTimeLimit(timeLimit);
}

}