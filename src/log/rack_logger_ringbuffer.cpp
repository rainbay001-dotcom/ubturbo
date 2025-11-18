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
#include "rack_logger_ringbuffer.h"

namespace turbo::log {
SpinLock::SpinLock(std::atomic_flag &flag) : lock(flag)
{
    while (lock.test_and_set(std::memory_order_acquire)) {
    }
}

SpinLock::~SpinLock()
{
    lock.clear(std::memory_order_release);
}

RingBuffer::RingBuffer(uint32_t size)
    : size(size), wIndex(0), count(0)
{
    buffer.resize(size);
}

RingBuffer::~RingBuffer()
{
    buffer.clear();
}

bool RingBuffer::IsEmpty() const
{
    return count.load(std::memory_order_relaxed) == 0;
}

void RingBuffer::SwapAtomic(std::atomic<unsigned int>& src, std::atomic<unsigned int>& dest)
{
    auto oldSrc = src.load(std::memory_order_acquire);
    auto oldDest = dest.load(std::memory_order_acquire);
    dest.store(oldSrc, std::memory_order_release);
    src.store(oldDest, std::memory_order_release);
}

void RingBuffer::Swap(RingBuffer &buffer, RingBuffer &other)
{
    std::swap(buffer.size, other.size);
    SwapAtomic(buffer.count, other.count);
    std::swap(buffer.buffer, other.buffer);
    SwapAtomic(buffer.wIndex, other.wIndex);
}

void RingBuffer::Push(TurboLoggerEntry &&loggerEntry)
{
    uint32_t writeIndex = wIndex.fetch_add(1, std::memory_order_relaxed) % size;
    Item item;
    item.isWriting = true;
    item.loggerEntry = std::move(loggerEntry);
    buffer[writeIndex] = std::move(item);
    uint32_t expect = 0;
    do {
        expect = count.load(std::memory_order_relaxed);
        if (expect >= size) {
            return;
        }
    } while (!count.compare_exchange_weak(expect, expect + 1, std::memory_order_relaxed, std::memory_order_relaxed));
}

bool RingBuffer::Pop(TurboLoggerEntry &loggerEntry)
{
    if (!IsEmpty()) {
        Item &item = buffer[wIndex % size];
        if (item.isWriting) {
            loggerEntry = std::move(item.loggerEntry);
            item.isWriting = false;
            wIndex.fetch_add(1, std::memory_order_relaxed);
            count.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }
    return false;
}

void RingBuffer::ResetIndex()
{
    wIndex.fetch_add(size);
    wIndex.fetch_sub(count);
    wIndex.store(wIndex % size);
}

void LogBuffer::Push(TurboLoggerEntry &&loggerEntry)
{
    // 需要支持多线程同时写入
    std::shared_lock<std::shared_mutex> lock(mtx);
    if (stop) {
        return;
    }
    writeBuffer.Push(std::move(loggerEntry));
}

bool LogBuffer::Pop(TurboLoggerEntry &loggerEntry)
{
    // 读为单线程操作
    std::unique_lock<std::shared_mutex> lock(mtx);
    if (readBuffer.IsEmpty()) {
        if (writeBuffer.IsEmpty()) {
            return false;
        } else {
            RingBuffer::Swap(readBuffer, writeBuffer);
            readBuffer.ResetIndex();
            writeBuffer.ResetIndex();
        }
    }
    return readBuffer.Pop(loggerEntry);
}
}