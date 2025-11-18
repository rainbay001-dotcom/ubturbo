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
#include <iostream>
#include <cstring>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>

#include "smap_interface.h"
#include "securec.h"
#include "turbo_def.h"
#include "../log/client/ulog.h"
#include "smap_handler_msg.h"

namespace turbo::smap::codec {

static void SmapDeleteData(uint8_t *data)
{
    delete[] data;
}

static void SmapResetBuf(TurboByteBuffer *buffer)
{
    if (buffer->data) {
        delete[] buffer->data;
        buffer->data = nullptr;
    }
    buffer->freeFunc = nullptr;
    buffer->len = 0;
}

int SmapMigrateOutCodec::EncodeRequest(TurboByteBuffer &buffer, MigrateOutMsg *msg, int pidType)
{
    size_t size = sizeof(int) + sizeof(MigrateOutMsg);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &pidType, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(int), size - sizeof(int), msg, sizeof(MigrateOutMsg));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapMigrateOutCodec::DecodeRequest(const TurboByteBuffer &buffer, MigrateOutMsg &msg, int &pidType)
{
    if (buffer.len < sizeof(int) + sizeof(MigrateOutMsg) || !buffer.data) {
        return -EINVAL;
    }
    pidType = *static_cast<int *>(static_cast<void *>(buffer.data));
    msg = *static_cast<MigrateOutMsg *>(static_cast<void *>(buffer.data + sizeof(int)));
    return 0;
}

int SmapMigrateOutCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapMigrateOutCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapMigrateBackCodec::EncodeRequest(TurboByteBuffer &buffer, MigrateBackMsg *msg)
{
    size_t size = sizeof(MigrateBackMsg);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, msg, size);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapMigrateBackCodec::DecodeRequest(const TurboByteBuffer &buffer, MigrateBackMsg &msg)
{
    if (buffer.len < sizeof(MigrateBackMsg)) {
        return -EINVAL;
    }
    msg = *static_cast<MigrateBackMsg *>(static_cast<void *>(buffer.data));
    return 0;
}

int SmapMigrateBackCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapMigrateBackCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapRemoveCodec::EncodeRequest(TurboByteBuffer &buffer, RemoveMsg *msg, int pidType)
{
    size_t size = sizeof(int) + sizeof(RemoveMsg);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &pidType, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(int), size - sizeof(int), msg, sizeof(RemoveMsg));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapRemoveCodec::DecodeRequest(const TurboByteBuffer &buffer, RemoveMsg &msg, int &pidType)
{
    if (buffer.len < sizeof(int) + sizeof(RemoveMsg)) {
        return -EINVAL;
    }
    pidType = *static_cast<int *>(static_cast<void *>(buffer.data));
    msg = *static_cast<RemoveMsg *>(static_cast<void *>(buffer.data + sizeof(int)));
    return 0;
}

int SmapRemoveCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapRemoveCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapEnableNodeCodec::EncodeRequest(TurboByteBuffer &buffer, EnableNodeMsg *msg)
{
    size_t size = sizeof(EnableNodeMsg);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, msg, sizeof(EnableNodeMsg));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapEnableNodeCodec::DecodeRequest(const TurboByteBuffer &buffer, EnableNodeMsg &msg)
{
    if (buffer.len < sizeof(EnableNodeMsg)) {
        return -EINVAL;
    }
    msg = *static_cast<EnableNodeMsg *>(static_cast<void *>(buffer.data));
    return 0;
}

int SmapEnableNodeCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapEnableNodeCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapInitCodec::EncodeRequest(TurboByteBuffer &buffer, uint32_t pageType)
{
    size_t size = sizeof(uint32_t);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &pageType, sizeof(uint32_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapInitCodec::DecodeRequest(const TurboByteBuffer &buffer, uint32_t &pageType)
{
    if (buffer.len < sizeof(uint32_t)) {
        return -EINVAL;
    }
    pageType = *static_cast<uint32_t *>(static_cast<void *>(buffer.data));
    return 0;
}

int SmapInitCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapInitCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapStopCodec::EncodeRequest(TurboByteBuffer &buffer)
{
    size_t size = sizeof(uint32_t);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    buffer.len = size;
    return 0;
}

int SmapStopCodec::DecodeRequest(const TurboByteBuffer &buffer)
{
    return 0;
}

int SmapStopCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapStopCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapUrgentMigrateOutCodec::EncodeRequest(TurboByteBuffer &buffer, uint64_t size)
{
    size_t s = sizeof(uint64_t);
    buffer.data = new (std::nothrow) uint8_t[s];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, s, &size, sizeof(uint64_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = s;
    return 0;
}

int SmapUrgentMigrateOutCodec::DecodeRequest(const TurboByteBuffer &buffer, uint64_t &size)
{
    if (buffer.len < sizeof(uint64_t)) {
        return -EINVAL;
    }
    size = *static_cast<uint64_t *>(static_cast<void *>(buffer.data));
    return 0;
}

int SmapUrgentMigrateOutCodec::EncodeResponse(TurboByteBuffer &buffer)
{
    size_t size = sizeof(uint32_t);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return 0;
}

int SmapUrgentMigrateOutCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    return 0;
}

int SmapAddProcessTrackingCodec::EncodeRequest(TurboByteBuffer &buffer, pid_t *pidArr, uint32_t *scanTime,
                                               uint32_t *duration, int len, int scanType)
{
    size_t size = buffer.len = sizeof(int) * 2 + sizeof(pid_t) * len + sizeof(uint32_t) * len + sizeof(uint32_t) * len;
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &len, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    size_t copied = sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, &scanType, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, pidArr, sizeof(pid_t) * len);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(pid_t) * len;
    ret = memcpy_s(buffer.data + copied, size - copied, scanTime, sizeof(uint32_t) * len);
    if (ret) {
        SmapResetBuf(&buffer);
    }
    copied += sizeof(uint32_t) * len;
    ret = memcpy_s(buffer.data + copied, size - copied, duration, sizeof(uint32_t) * len);
    if (ret) {
        SmapResetBuf(&buffer);
    }
    return ret;
}

int SmapAddProcessTrackingCodec::DecodeRequest(const TurboByteBuffer &buffer, pid_t *pidArr, uint32_t *scanTime,
                                               uint32_t *duration, int &len, int &scanType)
{
    if (buffer.len < sizeof(int) || !buffer.data) {
        return -EINVAL;
    }
    len = *static_cast<int *>(static_cast<void *>(buffer.data));
    if (buffer.len < sizeof(int) + sizeof(int) +
                     sizeof(pid_t) * len + sizeof(uint32_t) * len + sizeof(uint32_t) * len) {
        return -EINVAL;
    }
    size_t copied = sizeof(int);
    scanType = *static_cast<int *>(static_cast<void *>(buffer.data + copied));
    copied += sizeof(int);
    int ret = memcpy_s(pidArr, sizeof(pid_t) * MAX_NR_TRACKING, buffer.data + copied, sizeof(pid_t) * len);
    if (ret) {
        return ret;
    }
    copied += sizeof(pid_t) * len;
    ret = memcpy_s(scanTime, sizeof(uint32_t) * MAX_NR_TRACKING, buffer.data + copied, sizeof(uint32_t) * len);
    if (ret) {
        return ret;
    }
    copied += sizeof(uint32_t) * len;
    ret = memcpy_s(duration, sizeof(uint32_t) * MAX_NR_TRACKING, buffer.data + copied, sizeof(uint32_t) * len);
    if (ret) {
        return ret;
    }
    return ret;
}

int SmapAddProcessTrackingCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapAddProcessTrackingCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapRemoveProcessTrackingCodec::EncodeRequest(TurboByteBuffer &buffer, pid_t *pidArr, int len, int flag)
{
    size_t size = sizeof(int) * 2 + sizeof(pid_t) * len;
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &len, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    size_t copied = sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, &flag, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, pidArr, sizeof(pid_t) * len);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapRemoveProcessTrackingCodec::DecodeRequest(const TurboByteBuffer &buffer, pid_t *pidArr, int &len, int &flag)
{
    if (buffer.len < sizeof(int)) {
        return -EINVAL;
    }
    len = *static_cast<int *>(static_cast<void *>(buffer.data));
    if (buffer.len < sizeof(int) + sizeof(int) + sizeof(pid_t) * len) {
        return -EINVAL;
    }
    size_t copied = sizeof(int);
    flag = *static_cast<int *>(static_cast<void *>(buffer.data + copied));
    copied += sizeof(int);
    int ret = memcpy_s(pidArr, sizeof(pid_t) * MAX_NR_TRACKING, buffer.data + copied, sizeof(pid_t) * len);
    return ret;
}

int SmapRemoveProcessTrackingCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapRemoveProcessTrackingCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapEnableProcessMigrateCodec::EncodeRequest(TurboByteBuffer &buffer, pid_t *pidArr, int len, int enable, int flags)
{
    size_t size = buffer.len = sizeof(int) * 3 + sizeof(pid_t) * len;
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &len, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    size_t copied = sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, &enable, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, &flags, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, pidArr, sizeof(pid_t) * len);
    if (ret) {
        SmapResetBuf(&buffer);
    }
    return ret;
}

int SmapEnableProcessMigrateCodec::DecodeRequest(const TurboByteBuffer &buffer, pid_t *pidArr, int &len, int &enable,
                                                 int &flags)
{
    if (buffer.len < sizeof(int)) {
        return -EINVAL;
    }
    len = *static_cast<int *>(static_cast<void *>(buffer.data));
    if (buffer.len < sizeof(int) + sizeof(int) + sizeof(int) + sizeof(pid_t) * len) {
        return -EINVAL;
    }
    size_t copied = sizeof(int);
    enable = *static_cast<int *>(static_cast<void *>(buffer.data + copied));
    copied += sizeof(int);
    flags = *static_cast<int *>(static_cast<void *>(buffer.data + copied));
    copied += sizeof(int);
    int ret = memcpy_s(pidArr, sizeof(pid_t) * MAX_NR_TRACKING, buffer.data + copied, sizeof(pid_t) * len);
    return ret;
}

int SmapEnableProcessMigrateCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapEnableProcessMigrateCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SetSmapRemoteNumaInfoCodec::EncodeRequest(TurboByteBuffer &buffer, SetRemoteNumaInfoMsg *msg)
{
    size_t size = sizeof(SetRemoteNumaInfoMsg);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, msg, sizeof(SetRemoteNumaInfoMsg));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SetSmapRemoteNumaInfoCodec::DecodeRequest(const TurboByteBuffer &buffer, SetRemoteNumaInfoMsg &msg)
{
    if (buffer.len < sizeof(SetRemoteNumaInfoMsg)) {
        return -EINVAL;
    }
    msg = *static_cast<SetRemoteNumaInfoMsg *>(static_cast<void *>(buffer.data));
    return 0;
}

int SetSmapRemoteNumaInfoCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SetSmapRemoteNumaInfoCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapQueryVmFreqCodec::EncodeRequest(TurboByteBuffer &buffer, int pid, uint16_t lengthIn, int dataSource)
{
    size_t size = sizeof(int) + sizeof(uint16_t) + sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &pid, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(int), size - sizeof(int), &lengthIn, sizeof(uint16_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(int) + sizeof(uint16_t), size - sizeof(int) - sizeof(uint16_t),
        &dataSource, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapQueryVmFreqCodec::DecodeRequest(const TurboByteBuffer &buffer, int &pid, uint16_t &lengthIn, int &dataSource)
{
    if (buffer.len < sizeof(int) + sizeof(uint16_t) + sizeof(int)) {
        return -EINVAL;
    }
    pid = *static_cast<int *>(static_cast<void *>(buffer.data));
    lengthIn = *static_cast<uint16_t *>(static_cast<void *>(buffer.data + sizeof(int)));
    dataSource= *static_cast<uint16_t *>(static_cast<void *>(buffer.data + sizeof(int) + sizeof(uint16_t)));
    return 0;
}

int SmapQueryVmFreqCodec::EncodeResponse(TurboByteBuffer &buffer, uint16_t *data, uint16_t lengthOut, int returnValue)
{
    uint16_t length = lengthOut;
    if (length > MAX_NR_OF_QUERY_VM_FREQ_HCCS) {
        IPC_CLIENT_LOGGER_ERROR("LengthOut value %d is out of limit %d.\n", length, MAX_NR_OF_QUERY_VM_FREQ_HCCS);
        return -1;
    }
    size_t size = sizeof(int) + sizeof(uint16_t) + length * sizeof(uint16_t);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(int), size - sizeof(int), &length, sizeof(uint16_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    if (length) {
        ret = memcpy_s(buffer.data + sizeof(int) + sizeof(uint16_t), size - sizeof(int) - sizeof(uint16_t), data,
                       length * sizeof(uint16_t));
        if (ret) {
            SmapResetBuf(&buffer);
            return ret;
        }
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapQueryVmFreqCodec::DecodeResponse(const TurboByteBuffer &buffer, uint16_t *data, uint16_t &lengthOut,
                                         int &returnValue)
{
    if (buffer.len < sizeof(int) + sizeof(uint16_t)) {
        return IPC_ERROR;
    }
    returnValue = *static_cast<int *>(static_cast<void *>(buffer.data));
    uint16_t length = *static_cast<uint16_t *>(static_cast<void *>(buffer.data + sizeof(int)));
    if (length > MAX_NR_OF_QUERY_VM_FREQ_HCCS) {
        IPC_CLIENT_LOGGER_ERROR("Length: %d is out of limit %d.\n", length, MAX_NR_OF_QUERY_VM_FREQ_HCCS);
        return IPC_ERROR;
    }
    int ret = IPC_OK;
    if (length) {
        if (buffer.len < sizeof(int) + sizeof(uint16_t) + length * sizeof(uint16_t)) {
            IPC_CLIENT_LOGGER_ERROR("Buffer len is less than the required value.\n");
            return IPC_ERROR;
        }
        ret = memcpy_s(data, length * sizeof(uint16_t), buffer.data + sizeof(int) + sizeof(uint16_t),
                       length * sizeof(uint16_t));
        if (ret) {
            return IPC_ERROR;
        }
    }
    lengthOut = length;
    return ret;
}

int SetSmapRunModeCodec::EncodeRequest(TurboByteBuffer &buffer, int runMode)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &runMode, size);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SetSmapRunModeCodec::DecodeRequest(const TurboByteBuffer &buffer, int &runMode)
{
    if (buffer.len < sizeof(int)) {
        return -EINVAL;
    }
    runMode = *static_cast<int *>(static_cast<void *>(buffer.data));
    return 0;
}

int SetSmapRunModeCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, size);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SetSmapRunModeCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapIsRunningCodec::EncodeRequest(TurboByteBuffer &buffer)
{
    size_t size = sizeof(uint32_t);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    buffer.len = size;
    return 0;
}

int SmapIsRunningCodec::DecodeRequest(const TurboByteBuffer &buffer)
{
    return 0;
}

int SmapIsRunningCodec::EncodeResponse(TurboByteBuffer &buffer, bool returnValue)
{
    size_t size = sizeof(bool);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, size);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

bool SmapIsRunningCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(bool)) {
        return IPC_ERROR;
    }
    return *static_cast<bool *>(static_cast<void *>(buffer.data));
}

int SmapMigrateOutSyncCodec::EncodeRequest(TurboByteBuffer &buffer, MigrateOutMsg *msg, int pidType,
                                           uint64_t maxWaitTime)
{
    size_t size = sizeof(int) + sizeof(uint64_t) + sizeof(MigrateOutMsg);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &pidType, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(int), size - sizeof(int), &maxWaitTime, sizeof(uint64_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(int) + sizeof(uint64_t), size - sizeof(int) - sizeof(uint64_t), msg,
                   sizeof(MigrateOutMsg));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapMigrateOutSyncCodec::DecodeRequest(const TurboByteBuffer &buffer, MigrateOutMsg &msg, int &pidType,
                                           uint64_t &maxWaitTime)
{
    if (buffer.len < sizeof(int) + sizeof(uint64_t) + sizeof(MigrateOutMsg)) {
        return -EINVAL;
    }
    pidType = *static_cast<int *>(static_cast<void *>(buffer.data));
    maxWaitTime = *static_cast<uint64_t *>(static_cast<void *>(buffer.data + sizeof(int)));
    msg = *static_cast<MigrateOutMsg *>(static_cast<void *>(buffer.data + sizeof(int) + sizeof(uint64_t)));
    return 0;
}

int SmapMigrateOutSyncCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, size);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapMigrateOutSyncCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapMigrateRemoteNumaCodec::EncodeRequest(TurboByteBuffer &buffer, MigrateNumaMsg *msg)
{
    size_t size = sizeof(MigrateNumaMsg);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, msg, size);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    return ret;
}

int SmapMigrateRemoteNumaCodec::DecodeRequest(const TurboByteBuffer &buffer, MigrateNumaMsg &msg)
{
    if (buffer.len < sizeof(MigrateNumaMsg)) {
        return -EINVAL;
    }
    msg = *static_cast<MigrateNumaMsg *>(static_cast<void *>(buffer.data));
    return 0;
}

int SmapMigrateRemoteNumaCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, size);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapMigrateRemoteNumaCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapMigratePidRemoteNumaCodec::EncodeRequest(TurboByteBuffer &buffer, pid_t *pidArr, int len, int srcNid,
                                                 int destNid)
{
    size_t size = buffer.len = len * sizeof(pid_t) + 3 * sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &len, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    size_t copied = sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, &srcNid, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, &destNid, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, pidArr, len * sizeof(pid_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    return ret;
}

int SmapMigratePidRemoteNumaCodec::DecodeRequest(const TurboByteBuffer &buffer, pid_t *&pidArr, int &len, int &srcNid,
                                                 int &destNid)
{
    if (buffer.len < sizeof(int)) {
        return -EINVAL;
    }
    len = *static_cast<int *>(static_cast<void *>(buffer.data));
    if (buffer.len < sizeof(int) + sizeof(int) + sizeof(int) + len * sizeof(pid_t)) {
        return -EINVAL;
    }
    size_t copied = sizeof(int);
    srcNid = *static_cast<int *>(static_cast<void *>(buffer.data + sizeof(int)));
    copied += sizeof(int);
    destNid = *static_cast<int *>(static_cast<void *>(buffer.data + copied));
    copied += sizeof(int);
    pidArr = new (std::nothrow) pid_t[len];
    if (!pidArr) {
        return -EINVAL;
    }
    int ret = memcpy_s(pidArr, len * sizeof(pid_t), buffer.data + copied, len * sizeof(pid_t));
    if (ret) {
        delete[] pidArr;
        pidArr = nullptr;
    }
    return ret;
}

int SmapMigratePidRemoteNumaCodec::EncodeResponse(TurboByteBuffer &buffer, int returnValue)
{
    size_t size = sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, size);
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapMigratePidRemoteNumaCodec::DecodeResponse(TurboByteBuffer &buffer)
{
    if (buffer.len < sizeof(int)) {
        return IPC_ERROR;
    }
    return *static_cast<int *>(static_cast<void *>(buffer.data));
}

int SmapQueryProcessConfigCodec::EncodeRequest(TurboByteBuffer &buffer, int nid, int inLen)
{
    size_t size = buffer.len = sizeof(int) * 2;
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &nid, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(int), size - sizeof(int), &inLen, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
    }
    return ret;
}

int SmapQueryProcessConfigCodec::DecodeRequest(const TurboByteBuffer &buffer, int &nid, int &inLen)
{
    if (buffer.len < sizeof(int) + sizeof(int)) {
        return -EINVAL;
    }
    nid = *static_cast<int *>(static_cast<void *>(buffer.data));
    inLen = *static_cast<int *>(static_cast<void *>(buffer.data + sizeof(int)));
    return 0;
}

int SmapQueryProcessConfigCodec::EncodeResponse(TurboByteBuffer &buffer, struct ProcessPayload *result, int outLen,
                                                int returnValue)
{
    int out = outLen;
    if (out < 0 || out > MAX_NR_MIGOUT) {
        IPC_CLIENT_LOGGER_ERROR("EncodeResponse: out value %d is invalid; must be between 0 and %d\n",
                                out, MAX_NR_MIGOUT);
        return -1;
    }
    size_t size = sizeof(int) + sizeof(int) + sizeof(struct ProcessPayload) * out;
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    size_t copied = sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, &out, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(int);
    if (out) {
        ret = memcpy_s(buffer.data + copied, size - copied, result, out * sizeof(ProcessPayload));
        if (ret) {
            SmapResetBuf(&buffer);
            return ret;
        }
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapQueryProcessConfigCodec::DecodeResponse(TurboByteBuffer &buffer, struct ProcessPayload *result, int &outLen,
                                                int &returnValue)
{
    int ret = IPC_OK;
    if (buffer.len < sizeof(int) + sizeof(int)) {
        return IPC_ERROR;
    }
    returnValue = *static_cast<int *>(static_cast<void *>(buffer.data));
    int out = *static_cast<int *>(static_cast<void *>(buffer.data + sizeof(int)));
    if (out < 0 || out > MAX_NR_MIGOUT) {
        IPC_CLIENT_LOGGER_ERROR("DecodeResponse: out value %d is invalid; must be between 0 and %d\n",
                                out, MAX_NR_MIGOUT);
        return IPC_ERROR;
    }
    if (out) {
        if (buffer.len < sizeof(int) + sizeof(int) + out * sizeof(struct ProcessPayload)) {
            return IPC_ERROR;
        }
        ret = memcpy_s(result, out * sizeof(struct ProcessPayload), buffer.data + sizeof(int) + sizeof(int),
                       out * sizeof(struct ProcessPayload));
        if (ret) {
            return IPC_ERROR;
        }
    }
    outLen = out;
    return ret;
}

int SmapQueryRemoteNumaFreqCodec::EncodeRequest(TurboByteBuffer &buffer, uint16_t *numa, uint16_t length)
{
    size_t size = buffer.len = length * sizeof(uint16_t) + sizeof(uint16_t);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &length, sizeof(uint16_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    ret = memcpy_s(buffer.data + sizeof(uint16_t), size - sizeof(uint16_t), numa, length * sizeof(uint16_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    return ret;
}

int SmapQueryRemoteNumaFreqCodec::DecodeRequest(const TurboByteBuffer &buffer, uint16_t *&numa, uint16_t &length)
{
    if (buffer.len < sizeof(uint16_t)) {
        return -EINVAL;
    }
    length = *static_cast<uint16_t *>(static_cast<void *>(buffer.data));
    if (buffer.len < sizeof(uint16_t) + length * sizeof(uint16_t)) {
        return -EINVAL;
    }
    numa = new (std::nothrow) uint16_t[length];
    if (!numa) {
        return -EINVAL;
    }
    size_t copied = sizeof(uint16_t);
    int ret = memcpy_s(numa, length * sizeof(uint16_t), buffer.data + copied, length * sizeof(uint16_t));
    if (ret) {
        delete[] numa;
        numa = nullptr;
    }
    return ret;
}

int SmapQueryRemoteNumaFreqCodec::EncodeResponse(TurboByteBuffer &buffer, uint64_t *freq, uint16_t len, int returnValue)
{
    if (len > REMOTE_NUMA_BITS) {
        IPC_CLIENT_LOGGER_ERROR("DecodeResponse: out value %d is invalid; must be between 0 and %d\n",
                                len, REMOTE_NUMA_BITS);
        return -1;
    }
    size_t size = sizeof(uint16_t) + sizeof(uint64_t) * len + sizeof(int);
    buffer.data = new (std::nothrow) uint8_t[size];
    if (!buffer.data) {
        return -EINVAL;
    }
    int ret = memcpy_s(buffer.data, size, &returnValue, sizeof(int));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    size_t copied = sizeof(int);
    ret = memcpy_s(buffer.data + copied, size - copied, &len, sizeof(uint16_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    copied += sizeof(uint16_t);
    ret = memcpy_s(buffer.data + copied, size - copied, freq, len * sizeof(uint64_t));
    if (ret) {
        SmapResetBuf(&buffer);
        return ret;
    }
    buffer.len = size;
    buffer.freeFunc = SmapDeleteData;
    return ret;
}

int SmapQueryRemoteNumaFreqCodec::DecodeResponse(TurboByteBuffer &buffer, uint64_t *freq, uint16_t &outLen,
                                                 int &returnValue)
{
    int ret = IPC_OK;
    if (buffer.len < sizeof(int) + sizeof(uint16_t)) {
        return IPC_ERROR;
    }
    returnValue = *static_cast<int *>(static_cast<void *>(buffer.data));
    uint16_t out = *static_cast<uint16_t *>(static_cast<void *>(buffer.data + sizeof(int)));
    if (out > REMOTE_NUMA_BITS) {
        IPC_CLIENT_LOGGER_ERROR("DecodeResponse: out value %d is invalid; must be between 0 and %d\n",
                                out, REMOTE_NUMA_BITS);
        return IPC_ERROR;
    }
    if (buffer.len < sizeof(int) + sizeof(uint16_t) + out * sizeof(uint64_t)) {
        return IPC_ERROR;
    }
    ret = memcpy_s(freq, out * sizeof(uint64_t), buffer.data + sizeof(int) + sizeof(uint16_t),
                   out * sizeof(uint64_t));
    if (ret) {
        return IPC_ERROR;
    }
    outLen = out;
    return ret;
}

}
