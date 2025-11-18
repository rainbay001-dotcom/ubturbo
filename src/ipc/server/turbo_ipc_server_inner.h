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
#ifndef TURBO_IPC_SERVER_INNER_H
#define TURBO_IPC_SERVER_INNER_H

#include <string>
#include <functional>
#include "turbo_common.h"
#include "turbo_def.h"

namespace turbo::ipc::server {

using namespace turbo::common;

class IpcServerInner {
public:
    /*
    @brief 获取IpcServer单例对象
    @return IpcServer单例对象
    */
    static IpcServerInner &Instance();
    /*
    @brief 初始化IPC模块
    @return 0代表成功，非0代表失败
    */
    RetCode Init();
    /*
    @brief 启动IPC监听线程
    @return 0代表成功，非0代表失败
    */
    RetCode StartListen();
    /*
    @brief 结束IPC监听线程
    @return：0代表成功，非0代表失败
    */
    RetCode EndListen();
    /*
    @brief 注册回调函数
    @param name: 函数名
           function: 回调函数
    @return 0代表成功，非0代表失败
    */
    RetCode UBTurboRegIpcService(const std::string &name, IpcHandlerFunc function);
    /*
    @brief 取消注册回调函数
    @param name: 函数名
    @return 0代表成功，非0代表失败
    */
    RetCode UBTurboUnRegIpcService(const std::string &name);
};

}

#endif