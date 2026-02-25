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

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "smap_user_log.h"
#include "thread.h"
#include "manage.h"
#include "access_ioctl.h"

int AccessIoctlAddPid(int len, struct AccessAddPidPayload *payload)
{
    if (len <= 0 || len > GetCurrentMaxNrPid()) {
        SMAP_LOGGER_ERROR("AccessIoctlAddPid: invalid len %d.", len);
        return -EINVAL;
    }
    struct AccessAddPidMsg accessMsg = { .count = len };
    accessMsg.payload = malloc(len * sizeof(struct AccessAddPidPayload));
    if (accessMsg.payload == NULL) {
        SMAP_LOGGER_ERROR("AccessIoctlAddPid: malloc failed.");
        return -ENOMEM;
    }
    struct ProcessManager *manager = GetProcessManager();
    ThreadCtx *ctx = manager->threadCtx[0];
    uint32_t migrationPeriod = ctx ? ctx->period : LIGHT_STABLE_MIGRATE_CYCLE;

    for (int i = 0; i < len; i++) {
        accessMsg.payload[i].pid = payload[i].pid;
        accessMsg.payload[i].numaNodes = payload[i].numaNodes;
        if (payload[i].scanTime == 0) {
            SMAP_LOGGER_ERROR("AccessIoctlAddPid: invalid payload[%d].scanTime %u.", i, payload[i].scanTime);
            free(accessMsg.payload);
            return -EINVAL;
        }
        accessMsg.payload[i].scanTime = payload[i].scanTime;
        accessMsg.payload[i].duration = payload[i].duration;
        accessMsg.payload[i].nTimes = migrationPeriod / payload[i].scanTime;
        accessMsg.payload[i].type = payload[i].type;
    }
    int ret = ioctl(manager->fds.access, SMAP_ACCESS_ADD_PID, &accessMsg);
    free(accessMsg.payload);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("access ioctl add pids error: %d.", ret);
        ret = -EBADF;
    }
    return ret;
}

int AccessIoctlRemovePid(int len, struct AccessRemovePidPayload *payload)
{
    if (len <= 0 || len > GetCurrentMaxNrPid()) {
        SMAP_LOGGER_ERROR("AccessIoctlRemovePid: invalid len %d.", len);
        return -EINVAL;
    }
    struct AccessRemovePidMsg accessMsg = { .count = len };
    accessMsg.payload = malloc(len * sizeof(struct AccessRemovePidPayload));
    if (accessMsg.payload == NULL) {
        SMAP_LOGGER_ERROR("AccessIoctlRemovePid: malloc failed.");
        return -ENOMEM;
    }
    struct ProcessManager *manager = GetProcessManager();

    for (int i = 0; i < len; i++) {
        accessMsg.payload[i].pid = payload[i].pid;
    }
    int ret = ioctl(manager->fds.access, SMAP_ACCESS_REMOVE_PID, &accessMsg);
    free(accessMsg.payload);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("access ioctl remove pid error: %d.", ret);
        ret = -EBADF;
    }
    return ret;
}

int AccessIoctlRemoveAllPid(void)
{
    struct ProcessManager *manager = GetProcessManager();

    int ret = ioctl(manager->fds.access, SMAP_ACCESS_REMOVE_ALL_PID, 0);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("access ioctl remove all pid error: %d.", ret);
        ret = -EBADF;
    }
    return ret;
}

int AccessIoctlWalkPagemap(size_t *len)
{
    struct ProcessManager *manager = GetProcessManager();

    int ret = ioctl(manager->fds.access, SMAP_ACCESS_WALK_PAGEMAP, len);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("access walk pagemap error: %d.", ret);
        ret = -EBADF;
    }
    return ret;
}

int AccessIoctlReadPidFreq(struct AccessPidFreq *apf)
{
    struct ProcessManager *manager = GetProcessManager();

    int ret = ioctl(manager->fds.access, SMAP_ACCESS_READ_PID_FREQ, apf);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("access read pid freq error: %d\n", -errno);
        ret = -EBADF;
    }
    return ret;
}

int AccessRead(size_t len, char *buf)
{
    struct ProcessManager *manager = GetProcessManager();
    ssize_t bytesRead;
    size_t totalRead = 0;

    while (totalRead < len) {
        bytesRead = read(manager->fds.access, buf + totalRead, len - totalRead);
        if (bytesRead < 0) {
            SMAP_LOGGER_ERROR("access ioctl read error: %zd.", bytesRead);
            return -EIO;
        }
        SMAP_LOGGER_DEBUG("bytesRead: %zd, total len: %zu.", bytesRead, len);
        totalRead += bytesRead;
        if (bytesRead == 0) {
            break;
        }
    }
    if (totalRead != len) {
        SMAP_LOGGER_ERROR("access ioctl read expect %zu, actual %zd.", len, totalRead);
        return -EIO;
    }
    return 0;
}
