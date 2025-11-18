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
#ifndef TURBO_DEF_H
#define TURBO_DEF_H

#define IPC_OK 0
#define IPC_ERROR 1

#include <cstdint>
#include <functional>

using TurboByteBufferFreeFunc = std::function<void(uint8_t *data)>;

struct TurboByteBuffer {
    uint8_t *data = nullptr;  // 数据指针
    size_t len = 0;   // 数据长度
    TurboByteBufferFreeFunc freeFunc;  // 非空代表接收方需要释放内存；空代表接收方不需要释放内存
};

using IpcHandlerFunc = std::function<uint32_t(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)>;

#endif