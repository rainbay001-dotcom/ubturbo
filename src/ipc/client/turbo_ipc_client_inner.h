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
#ifndef TURBO_IPC_CLIENT_INNER_H
#define TURBO_IPC_CLIENT_INNER_H

#include <string>
#include <functional>
#include "turbo_def.h"

namespace turbo::ipc::client {

class IpcClientInner {
public:
    /*
    @brief 获取IpcClient单例对象
    @return IpcClient单例对象
    */
    static IpcClientInner &Instance();
    /*
    @brief 初始化IPC模块
    @return 0代表成功，非0代表失败
    */
    uint32_t Init();
    /*
    @brief 通过IPC通信调用UBTurbo的函数
    @param function: 函数名
           params: 入参，json格式的序列化后的参数
           result: 出参，json格式的序列化后的函数返回值
    @return 0代表成功，非0代表失败
    */
    uint32_t UBTurboFunctionCaller(const std::string &function, const TurboByteBuffer &params, TurboByteBuffer &result);
    uint32_t SetTimeLimit(uint32_t timeLimit);
private:
    bool CheckInputIsValid(const std::string &function, const TurboByteBuffer &params);
    uint32_t RecvMessage(int fd, TurboByteBuffer &result);
    uint32_t SendMessage(int fd, const std::string &function, const TurboByteBuffer &params);
    uint32_t ipcTimeLimit{60};
    static void SetHeader(uint8_t *message, uint32_t length);
    static uint32_t GetHeader(uint8_t *message);
};

}

#endif