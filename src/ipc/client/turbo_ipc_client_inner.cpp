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
#include "turbo_ipc_client_inner.h"

#include <arpa/inet.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <memory>
#include <iostream>

#include "securec.h"
#include "ulog.h"

namespace turbo::ipc::client {

IpcClientInner& IpcClientInner::Instance()
{
    static IpcClientInner instance;
    return instance;
}

uint32_t IpcClientInner::Init()
{
    return IPC_OK;
}

uint32_t IpcClientInner::SetTimeLimit(uint32_t timeLimit)
{
    ipcTimeLimit = timeLimit;
    return IPC_OK;
}

void IpcClientInner::SetHeader(uint8_t *message, uint32_t length)
{
    uint32_t tmp = htonl(length);
    if (memcpy_s(message, sizeof(uint32_t), &tmp, sizeof(uint32_t)) != 0) {
        return;
    }
    return;
}

uint32_t IpcClientInner::GetHeader(uint8_t *message)
{
    uint32_t ret;
    if (memcpy_s(&ret, sizeof(uint32_t), message, sizeof(uint32_t)) != 0) {
        return 0;
    }
    return ntohl(ret);
}

const char UDS_FILE_PATH[] = "/opt/ubturbo/ubturbo_ipc";
static const int BUFFER_LENGTH = 256;
static const int HEADER_LENGTH = 8;
static const int HEADER_OFFSET_LENGTH = 0;
static const int HEADER_OFFSET_STATUS = 4;
static const int MIN_MESSAGE_LENGTH = 8;
static const int MAX_MESSAGE_LENGTH = 1024 * 1024;
static const int MAX_FUNCTION_NAME_LENGTH = 128;

uint32_t IpcClientInner::SendMessage(int fd, const std::string &function, const TurboByteBuffer &params)
{
    IPC_CLIENT_LOGGER_DEBUG("[Ipc][Client] SendMessage start.");
    std::string name = function + " ";
    uint32_t sendBufferLength = params.len + HEADER_LENGTH + name.length();
    auto sendBuffer = std::make_unique<uint8_t[]>(sendBufferLength + 1);
    SetHeader(sendBuffer.get() + HEADER_OFFSET_LENGTH, sendBufferLength);
    SetHeader(sendBuffer.get() + HEADER_OFFSET_STATUS, name.length());
    if (memcpy_s(sendBuffer.get() + HEADER_LENGTH, sendBufferLength - HEADER_LENGTH,
        name.c_str(), name.length()) != 0) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Memory copy failed.");
        return IPC_ERROR;
    }
    if (params.len != 0 &&
        memcpy_s(sendBuffer.get() + HEADER_LENGTH + name.length(), params.len, params.data, params.len) != 0) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Memory copy failed.");
        return IPC_ERROR;
    }
    if (send(fd, sendBuffer.get(), sendBufferLength, 0) <= 0) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Send message failed: %s.", strerror(errno));
        return IPC_ERROR;
    }
    IPC_CLIENT_LOGGER_DEBUG("[Ipc][Client] SendMessage end.");
    return IPC_OK;
}

static void ClearBuffer(TurboByteBuffer &buffer)
{
    delete[] buffer.data;
    buffer.data = nullptr;
    buffer.len = 0;
}

uint32_t IpcClientInner::RecvMessage(int fd, TurboByteBuffer &result)
{
    IPC_CLIENT_LOGGER_DEBUG("[Ipc][Client] RecvMessage start.");
    struct timeval timeOut {ipcTimeLimit, 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeOut, sizeof(timeOut)) < 0) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Set socket option failed.");
        return IPC_ERROR;
    }

    auto receivedBuffer = std::make_unique<uint8_t[]>(BUFFER_LENGTH + 1);
    int receivedLength = recv(fd, receivedBuffer.get(), BUFFER_LENGTH, 0);
    if (receivedLength < MIN_MESSAGE_LENGTH) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] ReceivedLength is %d, min length is %d, max length is %d.",
            receivedLength, MIN_MESSAGE_LENGTH, MAX_MESSAGE_LENGTH);
        return IPC_ERROR;
    }
    receivedLength -= HEADER_LENGTH;
    int messageLength = static_cast<int>(GetHeader(receivedBuffer.get() + HEADER_OFFSET_LENGTH));
    if (messageLength < MIN_MESSAGE_LENGTH || messageLength > MAX_MESSAGE_LENGTH + HEADER_LENGTH) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Invalid message length.");
        return IPC_ERROR;
    }
    messageLength -= HEADER_LENGTH;
    result.data = new (std::nothrow) uint8_t[messageLength];
    if (result.data == nullptr) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Memory error.");
        return IPC_ERROR;
    }
    result.len = static_cast<uint32_t>(messageLength);
    if (messageLength != 0 &&
        memcpy_s(result.data, messageLength, receivedBuffer.get() + HEADER_LENGTH, receivedLength) != 0) {
        ClearBuffer(result);
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Memory copy failed.");
        return IPC_ERROR;
    }

    int totalReceived = receivedLength;
    while (totalReceived < messageLength) {
        receivedLength = recv(fd, result.data + totalReceived, messageLength - totalReceived, 0);
        if (receivedLength < 0) {
            ClearBuffer(result);
            IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Recv failed.");
            return IPC_ERROR;
        }
        totalReceived += receivedLength;
    }
    uint32_t retCode = GetHeader(receivedBuffer.get() + HEADER_OFFSET_STATUS);
    if (retCode != IPC_OK) {
        ClearBuffer(result);
    }
    IPC_CLIENT_LOGGER_DEBUG("[Ipc][Client] RecvMessage end.");
    return retCode;
}

bool IpcClientInner::CheckInputIsValid(const std::string &function, const TurboByteBuffer &params)
{
    if (function.find(' ') != std::string::npos) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Function name has invalid char.");
        return false;
    }
    if (function.length() > MAX_FUNCTION_NAME_LENGTH) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Function name is too long.");
        return false;
    }
    if (params.len > MAX_MESSAGE_LENGTH) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Invalid message length.");
        return false;
    }
    if (params.data == nullptr && params.len != 0) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Invalid message ptr.");
        return false;
    }
    return true;
}

uint32_t IpcClientInner::UBTurboFunctionCaller(const std::string &function, const TurboByteBuffer &params,
                                               TurboByteBuffer &result)
{
    IPC_CLIENT_LOGGER_INFO("[Ipc][Client] Start functioncall, func is %s.", function.c_str());
    if (!CheckInputIsValid(function, params)) {
        return IPC_ERROR;
    }
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Get socket failed: %s.", strerror(errno));
        return IPC_BAD_SOCKET;
    }
    struct sockaddr_un addr;
    if (memset_s(&addr, sizeof(addr), 0, sizeof(addr)) != 0) {
        close(fd);
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Memory set failed.");
        return IPC_ERROR;
    }
    addr.sun_family = AF_UNIX;
    if (strcpy_s(addr.sun_path, sizeof(addr.sun_path), UDS_FILE_PATH) != 0) {
        close(fd);
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] String copy failed.");
        return IPC_ERROR;
    }
    if (connect(fd, static_cast<struct sockaddr *>(static_cast<void *>(&addr)), sizeof(addr)) < 0) {
        close(fd);
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Connect failed: %s.", strerror(errno));
        return IPC_BAD_CONNECT;
    }
    if (SendMessage(fd, function, params) != IPC_OK) {
        close(fd);
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Send message failed.");
        return IPC_BAD_CONNECT;
    }
    uint32_t retCode = RecvMessage(fd, result);
    if (retCode != IPC_OK) {
        close(fd);
        IPC_CLIENT_LOGGER_ERROR("[Ipc][Client] Recv message failed.");
        return retCode;
    }
    close(fd);
    return IPC_OK;
}

}