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
#ifndef TURBO_LOGGER_BUFFER_H
#define TURBO_LOGGER_BUFFER_H

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "turbo_logger.h"

namespace turbo::log {
struct SpinLock {
    explicit SpinLock(std::atomic_flag &flag);
    ~SpinLock();

private:
    std::atomic_flag &lock;
};

class RingBuffer {
public:

    struct alignas(64) Item { // 64字节对齐，提高内存访问效率
        Item() : isWriting(false), loggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0) {}
        bool isWriting;
        TurboLoggerEntry loggerEntry;
    };

    explicit RingBuffer(uint32_t size);

    ~RingBuffer();

    bool IsEmpty() const;

    void Push(TurboLoggerEntry &&loggerEntry);
    bool Pop(TurboLoggerEntry &loggerEntry);

    void ResetIndex();

    static void SwapAtomic(std::atomic<unsigned int>& src, std::atomic<unsigned int>& dest);

    static void Swap(RingBuffer &buffer, RingBuffer &other);

    RingBuffer(RingBuffer const &) = delete;
    RingBuffer &operator = (RingBuffer const &) = delete;

public:
    uint32_t size;
    std::atomic<unsigned int> count;
    std::vector<Item> buffer{};
    std::atomic<unsigned int> wIndex; // 多生产者
};

class LogBuffer {
public:
    explicit LogBuffer(uint32_t size) : readBuffer(size), writeBuffer(size) {}

    void Push(TurboLoggerEntry &&loggerEntry);
    bool Pop(TurboLoggerEntry &loggerEntry);

public:
    std::shared_mutex mtx{};
    bool stop = false;

private:
    RingBuffer readBuffer;
    RingBuffer writeBuffer;
};

}
#endif // TURBO_LOGGER_BUFFER_H
