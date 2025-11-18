/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.‘
 */
#include "rmrs_smap_interface.h"
namespace {
int SmapEnableProcessMigrate(pid_t *pidArr, int len, int enable, int flags)
{
    return 0;
}

int SmapInit(uint32_t pageType, Logfunc extlog)
{
    return 0;
}

int SmapMigratePidRemoteNuma(pid_t *pidArr, int len, int srcNid, int destNid)
{
    return 0;
}

int SmapMigrateOut(struct MigrateOutMsg *msg, int pidType)
{
    return 0;
}

int SmapMigrateRemoteNuma(struct MigrateNumaMsg *msg)
{
    return 0;
}

int SmapQueryVmFreq(int pid, uint16_t *data, uint16_t lengthIn, uint16_t *lengthOut)
{
    return 0;
}

int SetSmapRemoteNumaInfo(struct SetRemoteNumaInfoMsg *msg)
{
    return 0;
}

int SmapMigrateBack(struct MigrateBackMsg *msg)
{
    return 0;
}

int SmapRemove(struct RemoveMsg *msg, int pidType)
{
    return 0;
}

int SmapMigrateOutSync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime)
{
    return 0;
}

int SetSmapRunMode(int runMode)
{
    return 0;
}

int SmapEnableNode(struct EnableNodeMsg *msg)
{
    return 0;
}

int SmapQueryNumaConfig(int nid, struct NumaPayload *result, int inLen, int *outLen)
{
    return 0;
}

int SmapQueryProcessConfig(int nid, struct ProcessPayload *result, int inLen, int *outLen)
{
    return 0;
}
}