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
#include "turbo_ipc_handler.h"

#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "securec.h"
#include "turbo_logger.h"
#include "turbo_error.h"

namespace turbo::ipc::server {

// Uds通信的文件路径
const char UDS_FILE_PATH[] = "/opt/ubturbo/ubturbo_ipc";
// listen的缓冲区大小
static const int LISTEN_BUFFER_LENGTH = 10;
static const int BUFFER_LENGTH = 256;
static const int HEADER_LENGTH = 8;
static const int HEADER_OFFSET_LENGTH = 0;
static const int HEADER_OFFSET_STATUS = 4;
static const int MIN_MESSAGE_LENGTH = 10;
static const int MAX_MESSAGE_LENGTH = 1024 * 1024 * 32;
static const int MAX_FUNCTION_NAME_LENGTH = 128;

IpcHandler& IpcHandler::Instance()
{
    static IpcHandler instance;
    return instance;
}

uint32_t IpcHandler::UBTurboRegIpcService(const std::string &name, IpcHandlerFunc function)
{
    std::lock_guard<std::shared_mutex> lock(gLock);
    if (name.empty()) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Name is empty.";
        return TURBO_ERROR;
    }
    if (!function) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Function is empty.";
        return TURBO_ERROR;
    }
    if (name.length() > MAX_FUNCTION_NAME_LENGTH) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Function name is too long.";
        return TURBO_ERROR;
    }
    if (funcTable.find(name) != funcTable.end()) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Function already registered.";
        return TURBO_ERROR;
    }
    funcTable[name] = function;
    return TURBO_OK;
}

uint32_t IpcHandler::UBTurboUnRegIpcService(const std::string &name)
{
    std::lock_guard<std::shared_mutex> lock(gLock);
    auto it = funcTable.find(name);
    if (it == funcTable.end()) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Function not found.";
        return TURBO_ERROR;
    }
    funcTable.erase(it);
    return TURBO_OK;
}

static const uint32_t SOCKET_MODE = 0640;

uint32_t IpcHandler::StartListen()
{
    std::lock_guard<std::shared_mutex> lock(gLock);
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Try start listen.";
    if (running) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Already start.";
        return TURBO_ERROR;
    }
    listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd < 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Get socket failed.";
        return TURBO_ERROR;
    }
    int flags = fcntl(listenFd, F_GETFL, 0);
    fcntl(listenFd, F_SETFL, static_cast<uint32_t>(flags) | O_NONBLOCK);
    if (memset_s(&addr, sizeof(addr), 0, sizeof(addr)) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Memory set failed.";
        close(listenFd);
        return TURBO_ERROR;
    }
    addr.sun_family = AF_UNIX;
    if (strcpy_s(addr.sun_path, sizeof(addr.sun_path), UDS_FILE_PATH) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] String copy failed.";
        close(listenFd);
        return TURBO_ERROR;
    }
    unlink(addr.sun_path);
    if (bind(listenFd, static_cast<struct sockaddr *>(static_cast<void *>(&addr)), sizeof(addr)) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Bind failed.";
        close(listenFd);
        return TURBO_ERROR;
    }
    if (chmod(addr.sun_path, SOCKET_MODE) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Chmod failed.";
        close(listenFd);
        return TURBO_ERROR;
    }
    if (listen(listenFd, LISTEN_BUFFER_LENGTH) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Listen failed.";
        close(listenFd);
        return TURBO_ERROR;
    }
    running = true;
    pThread = new std::thread([this]() {
        this->PThreadListen();
    });
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Start listen succeed.";
    return TURBO_OK;
}

uint32_t IpcHandler::EndListen()
{
    std::lock_guard<std::shared_mutex> lock(gLock);
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Try end listen.";
    if (!running) {
        UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Already end.";
        return TURBO_ERROR;
    }
    running = false;
    pThread->join();
    delete pThread;
    pThread = nullptr;
    close(listenFd);
    listenFd = -1;
    unlink(addr.sun_path);
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] End listen succeed.";
    return TURBO_OK;
}

void IpcHandler::PThreadListen()
{
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Thread for listen is running.";
    fd_set readFds;
    struct timeval timeOut;
    while (running) {
        FD_ZERO(&readFds);
        FD_SET(listenFd, &readFds);
        timeOut.tv_sec = 1;
        timeOut.tv_usec = 0;
        int maxSd = listenFd;
        int activity = select(maxSd + 1, &readFds, nullptr, nullptr, &timeOut);
        if (activity < 0) {
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Select error.";
            continue;
        }
        if (activity == 0) {
            continue;
        }
        if (!FD_ISSET(listenFd, &readFds)) {
            continue;
        }
        int fd = accept(listenFd, nullptr, nullptr);
        if (fd < 0) {
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Accept error.";
            continue;
        }
        UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Get a connect request.";
        std::thread handleThread([this, fd]() {
            PThreadHandle(fd);
        });
        handleThread.detach();
    }
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Thread for listen finished.";
}

static uint32_t GetHeader(uint8_t *message)
{
    uint32_t ret;
    if (memcpy_s(&ret, sizeof(uint32_t), message, sizeof(uint32_t)) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Memory copy error.";
        return 0;
    }
    return ntohl(ret);
}

static void SetHeader(uint8_t *message, uint32_t number)
{
    uint32_t tmp = htonl(number);
    if (memcpy_s(message, sizeof(uint32_t), &tmp, sizeof(uint32_t)) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Memory copy error.";
        return ;
    }
    return ;
}

static void ClearBuffer(TurboByteBuffer &buffer)
{
    delete[] buffer.data;
    buffer.data = nullptr;
    buffer.len = 0;
}

static RetCode RecvMoreMessage(int fd, TurboByteBuffer &params, uint8_t *receivedBuffer,
    int &messageLength, int &totalReceived)
{
    if (messageLength != 0 &&
        memcpy_s(params.data, messageLength, receivedBuffer + HEADER_LENGTH, totalReceived) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Memory copy failed.";
        ClearBuffer(params);
        return TURBO_ERROR;
    }
    int receivedLength;
    while (totalReceived < messageLength) {
        receivedLength = recv(fd, params.data + totalReceived, messageLength - totalReceived, 0);
        if (receivedLength <= 0) {
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Recv error.";
            ClearBuffer(params);
            return TURBO_ERROR;
        }
        totalReceived += receivedLength;
    }
    if (totalReceived != messageLength) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Wrong message length.";
        ClearBuffer(params);
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

static RetCode RecvMessage(int fd, TurboByteBuffer &params)
{
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Recv Message start.";
    struct timeval timeOut{1, 0};
    std::unique_ptr<uint8_t[]> receivedBuffer = std::make_unique<uint8_t[]>(BUFFER_LENGTH + 1);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeOut, sizeof(timeOut)) < 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Set socket option error.";
        return TURBO_ERROR;
    }
    int receivedLength = recv(fd, receivedBuffer.get(), BUFFER_LENGTH, 0);
    if (receivedLength <= 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Recv error.";
        return TURBO_ERROR;
    }
    if (receivedLength < MIN_MESSAGE_LENGTH) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Invalid message.";
        return TURBO_ERROR;
    }
    int messageLength = static_cast<int>(GetHeader(receivedBuffer.get() + HEADER_OFFSET_LENGTH));
    int nameLength = static_cast<int>(GetHeader(receivedBuffer.get() + HEADER_OFFSET_STATUS));
    if (nameLength < 0 || nameLength > MAX_FUNCTION_NAME_LENGTH) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Function name is too long.";
        return TURBO_ERROR;
    }
    if (messageLength < MIN_MESSAGE_LENGTH || messageLength > MAX_MESSAGE_LENGTH + HEADER_LENGTH + nameLength) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Invalid message legnth.";
        return TURBO_ERROR;
    }
    messageLength -= HEADER_LENGTH;
    params.data = new (std::nothrow) uint8_t[messageLength];
    if (params.data == nullptr) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Memory error.";
        return TURBO_ERROR;
    }
    params.len = static_cast<uint32_t>(messageLength);
    int totalReceived = receivedLength - HEADER_LENGTH;
    if (RecvMoreMessage(fd, params, receivedBuffer.get(), messageLength, totalReceived) != TURBO_OK) {
        return TURBO_ERROR;
    }
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Recv message end.";
    return TURBO_OK;
}

RetCode IpcHandler::HandleFunction(const std::string &functionName, const TurboByteBuffer &messageBuffer, int fd)
{
    gLock.lock_shared();
    auto it = funcTable.find(functionName);
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Fit function " << functionName << ".";
    TurboByteBuffer inputBuffer;
    inputBuffer.data = messageBuffer.data + functionName.length() + 1;
    inputBuffer.len = messageBuffer.len - functionName.length() - 1;
    TurboByteBuffer outputBuffer;
    RetCode retCode = IPC_OK;
    if (it != funcTable.end()) {
        if (it->second(inputBuffer, outputBuffer) != 0) {
            retCode = IPC_FUNC_ERROR;
        }
    } else {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Unknown function " << functionName << ".";
        retCode = IPC_NO_FUNC;
    }
    gLock.unlock_shared();

    if (retCode == IPC_NO_FUNC || retCode == IPC_FUNC_ERROR) {
        outputBuffer.len = 0;
    }
    if (outputBuffer.len > MAX_MESSAGE_LENGTH - HEADER_LENGTH ||
        (outputBuffer.len != 0 && outputBuffer.data == nullptr)) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Invalid output length.";
        retCode = IPC_INVALID_RESULT;
        outputBuffer.len = 0;
    }
    auto sendBuffer = std::make_unique<uint8_t[]>(outputBuffer.len + HEADER_LENGTH + 1);
    if (outputBuffer.len != 0 &&
        memcpy_s(sendBuffer.get() + HEADER_LENGTH, outputBuffer.len, outputBuffer.data, outputBuffer.len) != 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Memory copy failed.";
        if (outputBuffer.freeFunc) {
            outputBuffer.freeFunc(outputBuffer.data);
        }
        return TURBO_ERROR;
    }
    if (outputBuffer.freeFunc) {
        outputBuffer.freeFunc(outputBuffer.data);
    }
    SetHeader(sendBuffer.get() + HEADER_OFFSET_LENGTH, outputBuffer.len + HEADER_LENGTH);
    SetHeader(sendBuffer.get() + HEADER_OFFSET_STATUS, retCode);
    if (send(fd, sendBuffer.get(), outputBuffer.len + HEADER_LENGTH, 0) <= 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Send failed.";
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

void IpcHandler::PThreadHandle(int fd)
{
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Thread for handle start.";
    TurboByteBuffer messageBuffer;
    auto ret = RecvMessage(fd, messageBuffer);
    if (ret != TURBO_OK) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Recv message failed.";
        close(fd);
        return;
    }
    std::string text;
    bool getFuncName = false;
    for (unsigned int i = 0; i < messageBuffer.len; i++) {
        if (messageBuffer.data[i] == ' ') {
            getFuncName = true;
            text = std::string(static_cast<char *>(static_cast<void *>(messageBuffer.data)), i);
            break;
        }
    }
    if (!getFuncName) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] No function name.";
        ClearBuffer(messageBuffer);
        close(fd);
        return ;
    }
    ret = HandleFunction(text, messageBuffer, fd);
    ClearBuffer(messageBuffer);
    if (ret != TURBO_OK) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Handle " << text << " function failed.";
        close(fd);
        return ;
    }
    close(fd);
    UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Ipc][Server] Thread for handle finished.";
    return ;
}

}