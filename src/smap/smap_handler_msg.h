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
#ifndef SMAP_CODEC_H
#define SMAP_CODEC_H

#include <iostream>
#include <cstring>
#include <sys/un.h>
#include <unistd.h>

#include "turbo_def.h"
#include "smap_interface.h"

namespace turbo::smap::codec {
class SmapMigrateOutCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, MigrateOutMsg *msg, int pidType);
    int DecodeRequest(const TurboByteBuffer &buffer, MigrateOutMsg &msg, int &pidType);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapMigrateBackCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, MigrateBackMsg *msg);
    int DecodeRequest(const TurboByteBuffer &buffer, MigrateBackMsg &msg);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapRemoveCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, RemoveMsg *msg, int pidType);
    int DecodeRequest(const TurboByteBuffer &buffer, RemoveMsg &msg, int &pidType);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapEnableNodeCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, EnableNodeMsg *msg);
    int DecodeRequest(const TurboByteBuffer &buffer, EnableNodeMsg &msg);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapInitCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, uint32_t pageType);
    int DecodeRequest(const TurboByteBuffer &buffer, uint32_t &pageType);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapStopCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer);
    int DecodeRequest(const TurboByteBuffer &buffer);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapUrgentMigrateOutCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, uint64_t size);
    int DecodeRequest(const TurboByteBuffer &buffer, uint64_t &size);
    int EncodeResponse(TurboByteBuffer &buffer);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapAddProcessTrackingCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, pid_t *pidArr, uint32_t *scanTime, uint32_t *duration,
        int len, int scanType);
    int DecodeRequest(const TurboByteBuffer &buffer, pid_t *pidArr, uint32_t *scanTime, uint32_t *duration,
        int &len, int &scanType);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapRemoveProcessTrackingCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, pid_t *pidArr, int len, int flag);
    int DecodeRequest(const TurboByteBuffer &buffer, pid_t *pidArr, int &len, int &flag);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapEnableProcessMigrateCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, pid_t *pidArr, int len, int enable, int flags);
    int DecodeRequest(const TurboByteBuffer &buffer, pid_t *pidArr, int &len, int &enable, int &flags);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SetSmapRemoteNumaInfoCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, SetRemoteNumaInfoMsg *msg);
    int DecodeRequest(const TurboByteBuffer &buffer, SetRemoteNumaInfoMsg &msg);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapQueryVmFreqCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, int pid, uint32_t lengthIn, int dataSource);
    int DecodeRequest(const TurboByteBuffer &buffer, int &pid, uint32_t &lengthIn, int &dataSource);
    int EncodeResponse(TurboByteBuffer &buffer, uint16_t *data, uint32_t lengthOut, int returnValue);
    int DecodeResponse(const TurboByteBuffer &buffer, uint16_t *data, uint32_t &lengthOut, int &returnValue);
};

class SetSmapRunModeCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, int runMode);
    int DecodeRequest(const TurboByteBuffer &buffer, int &runMode);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapIsRunningCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer);
    int DecodeRequest(const TurboByteBuffer &buffer);
    int EncodeResponse(TurboByteBuffer &buffer, bool returnValue);
    bool DecodeResponse(TurboByteBuffer &buffer);
};

class SmapMigrateOutSyncCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime);
    int DecodeRequest(const TurboByteBuffer &buffer, MigrateOutMsg &msg, int &pidType, uint64_t &maxWaitTime);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapMigrateRemoteNumaCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, MigrateNumaMsg *msg);
    int DecodeRequest(const TurboByteBuffer &buffer, MigrateNumaMsg &msg);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapMigratePidRemoteNumaCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, MigrateEscapeMsg *msg);
    int DecodeRequest(const TurboByteBuffer &buffer, MigrateEscapeMsg &msg);
    int EncodeResponse(TurboByteBuffer &buffer, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer);
};

class SmapQueryProcessConfigCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, int nid, int inLen);
    int DecodeRequest(const TurboByteBuffer &buffer, int &nid, int &inLen);
    int EncodeResponse(TurboByteBuffer &buffer, struct ProcessPayload *result, int outLen, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer, struct ProcessPayload *result, int &outLen, int &returnValue);
};

class SmapQueryRemoteNumaFreqCodec {
public:
    int EncodeRequest(TurboByteBuffer &buffer, uint16_t *numa, uint16_t length);
    int DecodeRequest(const TurboByteBuffer &buffer, uint16_t *&numa, uint16_t &length);
    int EncodeResponse(TurboByteBuffer &buffer, uint64_t *freq, uint16_t len, int returnValue);
    int DecodeResponse(TurboByteBuffer &buffer, uint64_t *freq, uint16_t &outLen,
        int &returnValue);
};

}
#endif  // SMAP_CODEC_H