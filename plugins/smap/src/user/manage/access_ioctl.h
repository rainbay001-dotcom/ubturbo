/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef __ACCESS_IOCTL_H__
#define __ACCESS_IOCTL_H__

#include <sys/ioctl.h>
#include <linux/types.h>

#include "manage.h"

#define ACCESS_DEVICE "/dev/smap_access_device"
#define MAX_NR_PID MAX_NR_MIGOUT

struct AccessAddPidPayload {
    pid_t pid;
    uint32_t numaNodes;
    uint32_t scanTime;
    uint32_t duration;
    ScanType type;
    uint32_t nTimes;
};

struct AccessAddPidMsg {
    int count;
    struct AccessAddPidPayload *payload;
};

struct AccessRemovePidPayload {
    pid_t pid;
};

struct AccessRemovePidMsg {
    int count;
    struct AccessRemovePidPayload *payload;
};

struct AccessPidFreq {
    pid_t pid;
    size_t len[MAX_NODES];
    uint16_t *freq[MAX_NODES];
};

struct TrakingInfoPayload {
    pid_t pid;
    uint32_t length;
    uint16_t *data;
};

#define SMAP_ACCESS_MAGIC 0xBB
#define SMAP_ACCESS_ADD_PID _IOW(SMAP_ACCESS_MAGIC, 1, struct AccessAddPidMsg)
#define SMAP_ACCESS_REMOVE_PID _IOW(SMAP_ACCESS_MAGIC, 2, struct AccessRemovePidMsg)
#define SMAP_ACCESS_REMOVE_ALL_PID _IOW(SMAP_ACCESS_MAGIC, 3, int)
#define SMAP_ACCESS_WALK_PAGEMAP _IOW(SMAP_ACCESS_MAGIC, 4, size_t)
#define SMAP_ACCESS_GET_TRACKING _IOW(SMAP_ACCESS_MAGIC, 5, struct TrakingInfoPayload)
#define SMAP_ACCESS_READ_PID_FREQ _IOW(SMAP_ACCESS_MAGIC, 6, struct AccessPidFreq)

int AccessIoctlAddPid(int len, struct AccessAddPidPayload *payload);
int AccessIoctlRemovePid(int len, struct AccessRemovePidPayload *payload);
int AccessIoctlRemoveAllPid(void);
int AccessIoctlWalkPagemap(size_t *len);
int AccessIoctlReadPidFreq(struct AccessPidFreq *apf);
int AccessRead(size_t len, char *buf);

#endif /* __ACCESS_IOCTL_H__ */
