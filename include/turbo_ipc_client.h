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
#ifndef TURBO_IPC_CLIENT_H
#define TURBO_IPC_CLIENT_H

#include <string>
#include "turbo_def.h"

namespace turbo::ipc::client {

/*
@brief 通过IPC通信调用UBTurbo的函数
@param function: 函数名
        params: 入参，json格式的序列化后的参数
        result: 出参，json格式的序列化后的函数返回值
@return 0代表成功，非0代表失败
*/
extern "C" uint32_t UBTurboFunctionCaller(const std::string &function, const TurboByteBuffer &params,
                                          TurboByteBuffer &result);

extern "C" uint32_t SetIpcTimeLimit(uint32_t timeLimit);

} // namespace turbo::ipc::client

#endif