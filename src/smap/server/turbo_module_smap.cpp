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
#include "turbo_module_smap.h"

#include <unistd.h>
#include <fstream>
#include <dlfcn.h>
#include <filesystem>

#include "turbo_ipc_server.h"
#include "turbo_error.h"
#include "turbo_logger.h"
#include "client/ulog.h"
#include "smap_handler_msg.h"

using namespace turbo::smap::codec;
using namespace turbo::ipc::server;
using namespace turbo::smap::ulog;

namespace fs = std::filesystem;

namespace turbo::smap {

const std::string FILE_NAME = "/dev/shm/ubturbo_page_type.dat";

constexpr const char *LIB_SMAP_PATH = "/usr/lib64/libsmap.so";

static void *g_smapHandler = nullptr;
static SmapMigrateOutFunc g_smapMigrateOut = nullptr;
static SmapMigrateBackFunc g_smapMigrateBack = nullptr;
static SmapRemoveFunc g_smapRemove = nullptr;
static SmapEnableNodeFunc g_smapEnableNode = nullptr;
static SmapInitFunc g_smapInit = nullptr;
static SmapStopFunc g_smapStop = nullptr;
static SmapUrgentMigrateOutFunc g_smapUrgentMigrateOut = nullptr;
static SetSmapRemoteNumaInfoFunc g_setSmapRemoteNumaInfo = nullptr;
static SmapQueryFreqFunc g_smapQueryVmFreq = nullptr;
static SetSmapRunModeFunc g_setSmapRunMode = nullptr;
static SmapIsRunningFunc g_smapIsRunning = nullptr;
static SmapMigrateOutSyncFunc g_smapMigrateOutSync = nullptr;
static SmapAddProcessTrackingFunc g_smapAddProcessTracking = nullptr;
static SmapRemoveProcessTrackingFunc g_smapRemoveProcessTracking = nullptr;
static SmapEnableProcessMigrateFunc g_smapEnableProcessMigrate = nullptr;
static SmapMigrateRemoteNumaFunc g_smapMigrateRemoteNuma = nullptr;
static SmapMigratePidRemoteNumaFunc g_smapMigratePidRemoteNuma = nullptr;
static SmapQueryProcessConfigFunc g_smapQueryProcessConfig = nullptr;
static SmapQueryRemoteNumaFreqFunc g_smapQueryRemoteNumaFreq = nullptr;

RetCode SmapMigrateOutHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int pidType;
    MigrateOutMsg msg{};
    SmapMigrateOutCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, msg, pidType);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] ubturbo_smap_migrate_out DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapMigrateOut(&msg, pidType);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] ubturbo_smap_migrate_out EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapMigrateBackHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    MigrateBackMsg msg{};
    SmapMigrateBackCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, msg);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] ubturbo_smap_migrate_back DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapMigrateBack(&msg);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] ubturbo_smap_migrate_back EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapRemoveHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int pidType;
    RemoveMsg msg{};
    SmapRemoveCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, msg, pidType);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapRemoveHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapRemove(&msg, pidType);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapRemoveHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapEnableNodeHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    EnableNodeMsg msg{};
    SmapEnableNodeCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, msg);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapEnableNodeHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapEnableNode(&msg);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapEnableNodeHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

 void SmapToTurboLog(int level, const char *str, const char *moduleName)
{
    LoggerLevel logLevel = static_cast<LoggerLevel>(level);
    switch (logLevel) {
        case LoggerLevel::LOGGER_INFO_LEVEL:
            UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[" << moduleName << "]" << str;
            break;
        case LoggerLevel::LOGGER_WARNING_LEVEL:
            UBTURBO_LOG_WARN(MODULE_NAME, MODULE_CODE) << "[" << moduleName << "]" << str;
            break;
        case LoggerLevel::LOGGER_ERROR_LEVEL:
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[" << moduleName << "]" << str;
            break;
        default:
            UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[" << moduleName << "]" << str;
    }
}

const std::string SMAP_LOG_MODULE = "[Smap]";
 void SmapHandlerMsgToTurboLog(int level, const char *str, const char *moduleName)
{
    LoggerLevel logLevel = static_cast<LoggerLevel>(level);
    switch (logLevel) {
        case LoggerLevel::LOGGER_INFO_LEVEL:
            UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << SMAP_LOG_MODULE << str;
            break;
        case LoggerLevel::LOGGER_WARNING_LEVEL:
            UBTURBO_LOG_WARN(MODULE_NAME, MODULE_CODE) << SMAP_LOG_MODULE << str;
            break;
        case LoggerLevel::LOGGER_ERROR_LEVEL:
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << SMAP_LOG_MODULE << str;
            break;
        default:
            UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << SMAP_LOG_MODULE  << str;
    }
}

RetCode SmapInitHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    uint32_t pageType;
    SmapInitCodec codec;
    int retDecode = codec.DecodeRequest(inputBuffer, pageType);
    if (retDecode) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapInitHandler DecodeRequest error " << retDecode;
        return TURBO_ERROR;
    }
    RetCode ret = TurboModuleSmap::SavePageType(pageType);
    if (ret != TURBO_OK) {
        UBTURBO_LOG_WARN(MODULE_NAME, MODULE_CODE) << "[Smap] PageType could not be saved to file.";
    }
    int result = g_smapInit(pageType, SmapToTurboLog);
    int retEncode = codec.EncodeResponse(outputBuffer, result);
    if (retEncode) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapInitHandler EncodeResponse error " << retEncode;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapStopHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    SmapStopCodec codec;
    int result = g_smapStop();
    int ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapStopHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapUrgentMigrateOutHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    uint64_t size;
    SmapUrgentMigrateOutCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, size);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapUrgentMigrateOutHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    g_smapUrgentMigrateOut(size);
    codec.EncodeResponse(outputBuffer);
    return TURBO_OK;
}

RetCode SmapAddProcessTrackingHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int len;
    int ret;
    int scanType;
    pid_t pidArr[MAX_NR_TRACKING];
    uint32_t scanTime[MAX_NR_TRACKING];
    uint32_t duration[MAX_NR_TRACKING];
    SmapAddProcessTrackingCodec codec;
    ret = codec.DecodeRequest(inputBuffer, pidArr, scanTime, duration, len, scanType);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] SmapAddProcessTrackingHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapAddProcessTracking(pidArr, scanTime, duration, len, scanType);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapAddProcessTrackingHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapRemoveProcessTrackingHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int len;
    int flag;
    int ret;
    pid_t pidArr[MAX_NR_TRACKING];
    SmapRemoveProcessTrackingCodec codec;
    ret = codec.DecodeRequest(inputBuffer, pidArr, len, flag);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapRemoveProcessTrackingHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapRemoveProcessTracking(pidArr, len, flag);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapRemoveProcessTrackingHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapEnableProcessMigrateHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int len;
    int enable;
    int flags;
    int ret;
    pid_t pidArr[MAX_NR_TRACKING];
    SmapEnableProcessMigrateCodec codec;
    ret = codec.DecodeRequest(inputBuffer, pidArr, len, enable, flags);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapEnableProcessMigrateHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapEnableProcessMigrate(pidArr, len, enable, flags);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapEnableProcessMigrateHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SetSmapRemoteNumaInfoHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    SetRemoteNumaInfoMsg msg{};
    SetSmapRemoteNumaInfoCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, msg);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] SetSmapRemoteNumaInfoHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_setSmapRemoteNumaInfo(&msg);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] SetSmapRemoteNumaInfoHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapQueryFreqHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int pid;
    uint32_t lengthIn;
    int dataSource;
    SmapQueryVmFreqCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, pid, lengthIn, dataSource);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapQueryFreqHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    uint16_t data[lengthIn];
    uint32_t lengthOut = lengthIn;
    int result = g_smapQueryVmFreq(pid, data, lengthIn, &lengthOut, dataSource);
    ret = codec.EncodeResponse(outputBuffer, data, lengthOut, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapQueryFreqHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SetSmapRunModeHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int runMode;
    SetSmapRunModeCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, runMode);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SetSmapRunModeHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_setSmapRunMode(runMode);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SetSmapRunModeHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapIsRunningHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    SmapIsRunningCodec codec;
    bool result = g_smapIsRunning();
    int ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapIsRunningHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapMigrateOutSyncHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int pidType;
    uint64_t maxWaitTime;
    MigrateOutMsg msg{};
    SmapMigrateOutSyncCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, msg, pidType, maxWaitTime);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapMigrateOutSyncHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapMigrateOutSync(&msg, pidType, maxWaitTime);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] SmapMigrateOutSyncHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapMigrateRemoteNumaHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    MigrateNumaMsg msg{};
    SmapMigrateRemoteNumaCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, msg);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] SmapMigrateRemoteNumaHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapMigrateRemoteNuma(&msg);
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] SmapMigrateRemoteNumaHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapMigratePidRemoteNumaHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    pid_t *pidArr;
    int len;
    int srcNid;
    int destNid;
    int ret;
    SmapMigratePidRemoteNumaCodec codec;
    ret = codec.DecodeRequest(inputBuffer, pidArr, len, srcNid, destNid);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] SmapMigrateRemoteNumaHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    int result = g_smapMigratePidRemoteNuma(pidArr, len, srcNid, destNid);
    delete[] pidArr;
    ret = codec.EncodeResponse(outputBuffer, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] SmapMigrateRemoteNumaHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapQueryProcessConfigHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    int nid;
    int inLen;
    SmapQueryProcessConfigCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, nid, inLen);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] SmapQueryProcessConfigHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    if (inLen <= 0) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapQueryProcessConfigHandler DecodeRequest invalid inLen " << inLen;
        return TURBO_ERROR;
    }
    int outLen = 0;
    struct ProcessPayload payload[inLen];
    int result = g_smapQueryProcessConfig(nid, payload, inLen, &outLen);
    ret = codec.EncodeResponse(outputBuffer, payload, outLen, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapQueryProcessConfigHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

RetCode SmapQueryRemoteNumaFreqHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)
{
    uint16_t len;
    uint16_t *numa;
    SmapQueryRemoteNumaFreqCodec codec;
    int ret = codec.DecodeRequest(inputBuffer, numa, len);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapQueryRemoteNumaFreqHandler DecodeRequest error " << ret;
        return TURBO_ERROR;
    }
    uint64_t freq[len];
    int result = g_smapQueryRemoteNumaFreq(numa, freq, len);
    UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Smap] SmapQueryRemoteNumaFreqHandler result " << result;
    delete[] numa;
    ret = codec.EncodeResponse(outputBuffer, freq, len, result);
    if (ret) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) <<
                        "[Smap] SmapQueryRemoteNumaFreqHandler EncodeResponse error " << ret;
        return TURBO_ERROR;
    }
    return TURBO_OK;
}

#ifdef DT_CONFIG
int StubSmapMigrateOut(struct MigrateOutMsg *msg, int pidType)
{
    return 0;  // 模拟成功
}

int StubSmapMigrateBack(struct MigrateBackMsg *msg)
{
    return 0;
}

int StubSmapRemove(struct RemoveMsg *msg, int pidType)
{
    return 0;
}

int StubSmapEnableNode(struct EnableNodeMsg *msg)
{
    return 0;
}

int StubSmapInit(uint32_t pageType, Logfunc extlog)
{
    return 0;
}

int StubSmapStop(void)
{
    return 0;
}

void StubSmapUrgentMigrateOut(uint64_t size)
{
    return;
}

int StubSetSmapRemoteNumaInfo(struct SetRemoteNumaInfoMsg *msg)
{
    return 0;
}

int StubSmapQueryVmFreq(int pid, uint16_t *data, uint16_t lengthIn, uint16_t *lengthOut, int dataSource)
{
    return 0;
}

int StubSetSmapRunMode(int runMode)
{
    return 0;
}

bool StubSmapIsRunning()
{
    return true;
}

int StubSmapMigrateOutSync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime)
{
    return 0;
}

int StubSmapAddProcessTracking(pid_t *pidArr, uint32_t *scanTime, uint32_t *dataSource, int len, int scanType)
{
    return 0;
}

int StubSmapRemoveProcessTracking(pid_t *pidArr, int len, int flag)
{
    return 0;
}

int StubSmapEnableProcessMigrate(pid_t *pidArr, int len, int enable, int flags)
{
    return 0;
}

int StubSmapMigrateRemoteNuma(struct MigrateNumaMsg *msg)
{
    return 0;
}

int StubSmapMigratePidRemoteNuma(pid_t *pidArr, int len, int srcNid, int destNid)
{
    return 0;
}

int StubSmapQueryProcessConfig(int nid, struct ProcessPayload *result, int inLen, int *outLen)
{
    return 0;
}

int StubSmapQueryRemoteNumaFreq(uint16_t *numa, uint64_t *freq, uint16_t length)
{
    return 0;
}

void StubSmapPtr()
{
    g_smapMigrateOut = StubSmapMigrateOut;
    g_smapMigrateBack = StubSmapMigrateBack;
    g_smapRemove = StubSmapRemove;
    g_smapEnableNode = StubSmapEnableNode;
    g_smapInit = StubSmapInit;
    g_smapStop = StubSmapStop;
    g_smapUrgentMigrateOut = StubSmapUrgentMigrateOut;
    g_setSmapRemoteNumaInfo = StubSetSmapRemoteNumaInfo;
    g_smapQueryVmFreq = StubSmapQueryVmFreq;
    g_setSmapRunMode = StubSetSmapRunMode;
    g_smapIsRunning = StubSmapIsRunning;
    g_smapMigrateOutSync = StubSmapMigrateOutSync;
    g_smapAddProcessTracking = StubSmapAddProcessTracking;
    g_smapRemoveProcessTracking = StubSmapRemoveProcessTracking;
    g_smapEnableProcessMigrate = StubSmapEnableProcessMigrate;
    g_smapMigrateRemoteNuma = StubSmapMigrateRemoteNuma;
    g_smapMigratePidRemoteNuma = StubSmapMigratePidRemoteNuma;
    g_smapQueryProcessConfig = StubSmapQueryProcessConfig;
    g_smapQueryRemoteNumaFreq = StubSmapQueryRemoteNumaFreq;
}
#endif

int OpenSmapHandler()
{
    bool flag;
    g_smapHandler = dlopen(LIB_SMAP_PATH, RTLD_LAZY);
    if (!g_smapHandler) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] Cannot load library";
        return -ENOENT;
    }

    g_smapMigrateOut = (SmapMigrateOutFunc)dlsym(g_smapHandler, "ubturbo_smap_migrate_out");
    g_smapMigrateBack = (SmapMigrateBackFunc)dlsym(g_smapHandler, "ubturbo_smap_migrate_back");
    g_smapRemove = (SmapRemoveFunc)dlsym(g_smapHandler, "ubturbo_smap_remove");
    g_smapEnableNode = (SmapEnableNodeFunc)dlsym(g_smapHandler, "ubturbo_smap_node_enable");
    g_smapInit = (SmapInitFunc)dlsym(g_smapHandler, "ubturbo_smap_start");
    g_smapStop = (SmapStopFunc)dlsym(g_smapHandler, "ubturbo_smap_stop");
    g_smapUrgentMigrateOut = (SmapUrgentMigrateOutFunc)dlsym(g_smapHandler, "ubturbo_smap_urgent_migrate_out");
    g_setSmapRemoteNumaInfo = (SetSmapRemoteNumaInfoFunc)dlsym(g_smapHandler, "ubturbo_smap_remote_numa_info_set");
    g_smapQueryVmFreq = (SmapQueryFreqFunc)dlsym(g_smapHandler, "ubturbo_smap_freq_query");
    g_setSmapRunMode = (SetSmapRunModeFunc)dlsym(g_smapHandler, "ubturbo_smap_run_mode_set");
    g_smapIsRunning = (SmapIsRunningFunc)dlsym(g_smapHandler, "ubturbo_smap_is_running");
    g_smapMigrateOutSync = (SmapMigrateOutSyncFunc)dlsym(g_smapHandler, "ubturbo_smap_migrate_out_sync");
    g_smapAddProcessTracking = (SmapAddProcessTrackingFunc)dlsym(g_smapHandler, "ubturbo_smap_process_tracking_add");
    g_smapRemoveProcessTracking = (SmapRemoveProcessTrackingFunc)dlsym(g_smapHandler,
                                                                       "ubturbo_smap_process_tracking_remove");
    g_smapEnableProcessMigrate = (SmapEnableProcessMigrateFunc)dlsym(g_smapHandler,
                                                                     "ubturbo_smap_process_migrate_enable");
    g_smapMigrateRemoteNuma = (SmapMigrateRemoteNumaFunc)dlsym(g_smapHandler, "ubturbo_smap_remote_numa_migrate");
    g_smapMigratePidRemoteNuma = (SmapMigratePidRemoteNumaFunc)dlsym(g_smapHandler,
                                                                     "ubturbo_smap_pid_remote_numa_migrate");
    g_smapQueryProcessConfig = (SmapQueryProcessConfigFunc)dlsym(g_smapHandler, "ubturbo_smap_process_config_query");
    g_smapQueryRemoteNumaFreq = (SmapQueryRemoteNumaFreqFunc)dlsym(g_smapHandler,
                                                                   "ubturbo_smap_remote_numa_freq_query");
    flag = !g_smapMigrateOut || !g_smapMigrateBack || !g_smapRemove || !g_smapEnableNode || !g_smapInit ||
           !g_smapStop || !g_smapUrgentMigrateOut || !g_setSmapRemoteNumaInfo || !g_smapQueryVmFreq ||
           !g_setSmapRunMode || !g_smapIsRunning || !g_smapMigrateOutSync || !g_smapAddProcessTracking ||
           !g_smapRemoveProcessTracking || !g_smapEnableProcessMigrate || !g_smapMigrateRemoteNuma ||
           !g_smapMigratePidRemoteNuma || !g_smapQueryProcessConfig || !g_smapQueryRemoteNumaFreq;
    if (flag) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] Smap function not found";
        return -EINVAL;
    }
    return 0;
}

void CloseSmapHandler()
{
    if (g_smapHandler) {
        dlclose(g_smapHandler);
        g_smapHandler = nullptr;
        UBTURBO_LOG_DEBUG(MODULE_NAME, MODULE_CODE) << "[Smap] Close smap handler";
    }
}

void RegSmapHandler()
{
    UBTurboRegIpcService("ubturbo_smap_migrate_out", SmapMigrateOutHandler);
    UBTurboRegIpcService("ubturbo_smap_migrate_back", SmapMigrateBackHandler);
    UBTurboRegIpcService("ubturbo_smap_remove", SmapRemoveHandler);
    UBTurboRegIpcService("ubturbo_smap_node_enable", SmapEnableNodeHandler);
    UBTurboRegIpcService("ubturbo_smap_start", SmapInitHandler);
    UBTurboRegIpcService("ubturbo_smap_stop", SmapStopHandler);
    UBTurboRegIpcService("ubturbo_smap_urgent_migrate_out", SmapUrgentMigrateOutHandler);
    UBTurboRegIpcService("ubturbo_smap_process_tracking_add", SmapAddProcessTrackingHandler);
    UBTurboRegIpcService("ubturbo_smap_process_tracking_remove", SmapRemoveProcessTrackingHandler);
    UBTurboRegIpcService("ubturbo_smap_process_migrate_enable", SmapEnableProcessMigrateHandler);
    UBTurboRegIpcService("ubturbo_smap_remote_numa_info_set", SetSmapRemoteNumaInfoHandler);
    UBTurboRegIpcService("ubturbo_smap_freq_query", SmapQueryFreqHandler);
    UBTurboRegIpcService("ubturbo_smap_run_mode_set", SetSmapRunModeHandler);
    UBTurboRegIpcService("ubturbo_smap_is_running", SmapIsRunningHandler);
    UBTurboRegIpcService("ubturbo_smap_migrate_out_sync", SmapMigrateOutSyncHandler);
    UBTurboRegIpcService("ubturbo_smap_remote_numa_migrate", SmapMigrateRemoteNumaHandler);
    UBTurboRegIpcService("ubturbo_smap_pid_remote_numa_migrate", SmapMigratePidRemoteNumaHandler);
    UBTurboRegIpcService("ubturbo_smap_process_config_query", SmapQueryProcessConfigHandler);
    UBTurboRegIpcService("ubturbo_smap_remote_numa_freq_query", SmapQueryRemoteNumaFreqHandler);
}

void UnRegSmapHandler()
{
    UBTurboUnRegIpcService("ubturbo_smap_migrate_out");
    UBTurboUnRegIpcService("ubturbo_smap_migrate_back");
    UBTurboUnRegIpcService("ubturbo_smap_remove");
    UBTurboUnRegIpcService("ubturbo_smap_node_enable");
    UBTurboUnRegIpcService("ubturbo_smap_start");
    UBTurboUnRegIpcService("ubturbo_smap_stop");
    UBTurboUnRegIpcService("ubturbo_smap_urgent_migrate_out");
    UBTurboUnRegIpcService("ubturbo_smap_process_tracking_add");
    UBTurboUnRegIpcService("ubturbo_smap_process_tracking_remove");
    UBTurboUnRegIpcService("ubturbo_smap_process_migrate_enable");
    UBTurboUnRegIpcService("ubturbo_smap_remote_numa_info_set");
    UBTurboUnRegIpcService("ubturbo_smap_freq_query");
    UBTurboUnRegIpcService("ubturbo_smap_run_mode_set");
    UBTurboUnRegIpcService("ubturbo_smap_is_running");
    UBTurboUnRegIpcService("ubturbo_smap_migrate_out_sync");
    UBTurboUnRegIpcService("ubturbo_smap_remote_numa_migrate");
    UBTurboUnRegIpcService("ubturbo_smap_pid_remote_numa_migrate");
    UBTurboUnRegIpcService("ubturbo_smap_process_config_query");
    UBTurboUnRegIpcService("ubturbo_smap_remote_numa_freq_query");
}

RetCode TurboModuleSmap::Init()
{
    return TURBO_OK;
}

RetCode TurboModuleSmap::Start()
{
    if (OpenSmapHandler()) {
        return TURBO_ERROR;
    }
    RegSmapHandler();
    UpstreamSubscribeLogger(SmapHandlerMsgToTurboLog);

    if (!PageTypeFileExists()) {
        UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Smap] PageType file does not exist, not initializating smap.";
        return TURBO_OK;
    }
    uint32_t pageType;
    RetCode ret = LoadPageType(pageType);
    if (ret != TURBO_OK) {
        UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE)
            << "[Smap] PageType file exists but could not be loaded, not initializating smap.";
        return TURBO_OK;
    }

    int res = g_smapInit(pageType, SmapToTurboLog);
    if (res == 0) {
        UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Smap] ubturbo_smap_start scucess from pageType file.";
    } else if (res == -1) {
        UBTURBO_LOG_WARN(MODULE_NAME, MODULE_CODE) << "[Smap] ubturbo_smap_start already initialized.";
    } else if (res < -1) {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] ubturbo_smap_start failed, ret : " << res;
        return TURBO_ERROR;
    }

    return TURBO_OK;
}

void TurboModuleSmap::Stop()
{
    UnRegSmapHandler();
    CloseSmapHandler();
}

void TurboModuleSmap::UnInit() {}

std::string TurboModuleSmap::Name()
{
    return "Smap";
}

RetCode TurboModuleSmap::SavePageType(uint32_t pageType)
{
    std::ofstream outFile(FILE_NAME, std::ios::binary);
    if (outFile) {
        outFile.write(reinterpret_cast<const char *>(&pageType), sizeof(pageType));
        outFile.close();

        if (fs::is_symlink(FILE_NAME)) {
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
                << "[Smap] Cannot modify permissions of a symbolic link: " << FILE_NAME << ".";
            return TURBO_ERROR;
        }

        // 设置权限为 600 (owner read + write)
        std::error_code ec;
        fs::permissions(FILE_NAME,
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace, ec);
        if (ec) {
            UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] Failed to set file permissions: " << ec.message();
            return TURBO_ERROR;
        }

        UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Smap] PageType saved to file with permissions 600.";
    } else {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE) << "[Smap] Error opening file for writing.";
        return TURBO_ERROR;
    }

    return TURBO_OK;
}

RetCode TurboModuleSmap::LoadPageType(uint32_t &pageType)
{
    pageType = 0;
    std::ifstream inFile(FILE_NAME, std::ios::binary);
    if (inFile) {
        inFile.read(reinterpret_cast<char *>(&pageType), sizeof(pageType));
        inFile.close();
        UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Smap] PageType loaded from file: " << pageType << "\n";
    } else {
        UBTURBO_LOG_ERROR(MODULE_NAME, MODULE_CODE)
            << "[Smap] Error opening file for reading or file does not exist.\n";
        return TURBO_ERROR;
    }

    return TURBO_OK;
}

bool TurboModuleSmap::PageTypeFileExists()
{
    std::ifstream inFile(FILE_NAME);
    return inFile.good();
}
}