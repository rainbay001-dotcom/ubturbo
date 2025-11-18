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
#ifndef TURBO_SMAP_MODULE_H
#define TURBO_SMAP_MODULE_H

#include "turbo_common.h"
#include "turbo_module.h"
#include "turbo_def.h"
#include "smap_interface.h"

namespace turbo::smap {
using namespace turbo::common;
using namespace turbo::module;

enum class LoggerLevel : uint32_t {
    LOGGER_DEBUG_LEVEL = 0,
    LOGGER_INFO_LEVEL = 1,
    LOGGER_WARNING_LEVEL = 2,
    LOGGER_ERROR_LEVEL = 3
};

using SmapMigrateOutFunc = int (*)(struct MigrateOutMsg *msg, int pidType);
using SmapMigrateBackFunc = int (*)(struct MigrateBackMsg *msg);
using SmapRemoveFunc = int (*)(struct RemoveMsg *msg, int pidType);
using SmapEnableNodeFunc = int (*)(struct EnableNodeMsg *msg);
using SmapInitFunc = int (*)(uint32_t pageType, Logfunc extlog);
using SmapStopFunc = int (*)(void);
using SmapUrgentMigrateOutFunc = void (*)(uint64_t size);
using SetSmapRemoteNumaInfoFunc = int (*)(struct SetRemoteNumaInfoMsg *msg);
using SmapQueryVmFreqFunc = int (*)(int pid, uint16_t *data, uint16_t lengthIn, uint16_t *lengthOut, int dataSource);
using SetSmapRunModeFunc = int (*)(int runMode);
using SmapIsRunningFunc = bool (*)(void);
using SmapMigrateOutSyncFunc = int (*)(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime);
using SmapAddProcessTrackingFunc = int (*)(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration,
    int len, int scanType);
using SmapRemoveProcessTrackingFunc = int (*)(pid_t *pidArr, int len, int flag);
using SmapEnableProcessMigrateFunc = int (*)(pid_t *pidArr, int len, int enable, int flags);
using SmapMigrateRemoteNumaFunc = int (*)(struct MigrateNumaMsg *msg);
using SmapMigratePidRemoteNumaFunc = int (*)(pid_t *pidArr, int len, int srcNid, int destNid);
using SmapQueryProcessConfigFunc = int (*)(int nid, struct ProcessPayload *result, int inLen, int *outLen);
using SmapQueryRemoteNumaFreqFunc = int (*)(uint16_t *numa, uint64_t *freq, uint16_t length);

class TurboModuleSmap : public TurboModule {
public:
    RetCode Init() override;

    void UnInit() override;

    RetCode Start() override;

    void Stop() override;

    std::string Name() override;

    static RetCode SavePageType(uint32_t pageType);

    static RetCode LoadPageType(uint32_t &pageType);

    static bool PageTypeFileExists();
};

RetCode SmapMigrateOutHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapMigrateBackHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapRemoveHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapEnableNodeHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapInitHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapStopHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapUrgentMigrateOutHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapAddProcessTrackingHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapRemoveProcessTrackingHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapEnableProcessMigrateHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SetSmapRemoteNumaInfoHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapQueryVmFreqHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SetSmapRunModeHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapIsRunningHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapMigrateOutSyncHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapMigrateRemoteNumaHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapMigratePidRemoteNumaHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapQueryProcessConfigHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
RetCode SmapQueryRemoteNumaFreqHandler(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer);
int OpenSmapHandler();
void CloseSmapHandler();
void RegSmapHandler();
void UnRegSmapHandler();

#ifdef DT_CONFIG
void StubSmapPtr();
int StubSmapInit(uint32_t pageType, Logfunc extlog);
#endif
}
#endif  // TURBO_SMAP_MODULE_H