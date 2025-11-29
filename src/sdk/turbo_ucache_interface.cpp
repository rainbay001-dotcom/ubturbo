/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * ucache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "turbo_ucache_interface.h"
#include "turbo_error.h"
#include "turbo_ipc_client.h"
#include "turbo_serialize.h"
#include "ulog.h"

namespace turbo::ucache {
using namespace turbo::serialize;
using namespace turbo::ipc::client;

uint32_t UBTurboUCacheExecuteTask(const TaskRequest &tReq, TaskResponse &tResp)
{
    IPC_CLIENT_LOGGER_DEBUG("[UCache] Send UBTurboUCacheExecuteTask ipc message start.");
    TurboByteBuffer reqBuffer{};
    TurboByteBuffer respBuffer{};
    {
        RmrsOutStream out;
        out << tReq;
        reqBuffer.data = out.GetBufferPointer();
        reqBuffer.len = out.GetSize();
    }
    uint32_t ret = UBTurboFunctionCaller("UCacheExecuteTask", reqBuffer, respBuffer);
    delete[] reqBuffer.data;
    if (ret != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[UCache] Send UBTurboUCacheExecuteTask ipc message failed, res=%u.", ret);
        return ret;
    }
    {
        RmrsInStream in(respBuffer.data, respBuffer.len);
        in >> tResp;
    }
    delete[] respBuffer.data;
    IPC_CLIENT_LOGGER_DEBUG("[UCache] Send UBTurboUCacheExecuteTask ipc message succeed.");
    return TURBO_OK;
}
} // namespace turbo::ucache