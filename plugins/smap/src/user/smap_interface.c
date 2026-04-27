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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "securec.h"

#include "advanced-strategy/scene.h"
#include "smap_env.h"
#include "smap_user_log.h"
#include "manage/manage.h"
#include "manage/oom_migrate.h"
#include "manage/device.h"
#include "manage/thread.h"
#include "manage/access_ioctl.h"
#include "manage/smap_ioctl.h"
#include "manage/virt.h"
#include "manage/smap_config.h"
#include "strategy/migration.h"

#include "smap_interface.h"
#define DEFAULT_NODE_NUMBER_SIZE 16
#define REMOTE_NUMA_MEMORY_MAX (TIB / MIB)
#define NUMA_MAPS_MAX_PATTERN_LEN 20
#define LOCAL_NUMA_BIT_MAP_MASK 0xF

static EnvAtomic g_status;

inline bool ubturbo_smap_is_running(void)
{
    return EnvAtomicRead(&g_status) == RUNNING;
}

static int IoctlHandler(const void *msg)
{
    int fd = open(SMAP_DEVICE, O_RDWR);
    if (fd < 0) {
        SMAP_LOGGER_ERROR("cannot find %s, skipped.", SMAP_DEVICE);
        return -EBADF;
    }
    int ret = ioctl(fd, SMAP_MIGRATE_BACK, msg);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("ioctl failed, result: %d.", ret);
        ret = -EBADF;
    }
    close(fd);
    return ret;
}

static inline bool IsRatioValid(int ratio)
{
    return (ratio >= 0 && ratio <= HUNDRED);
}

static bool IsMigOutCountValid(pid_t *pidArr, int len, int pidType)
{
    int newNum = 0;
    int oldNum = (pidType == PAGETYPE_NORMAL ? LoadMangerNrProcessNum() : LoadMangerNrVmNum());
    for (int i = 0; i < len; i++) {
        ProcessAttr *attr = GetProcessAttrLocked(pidArr[i]);
        if (!attr) {
            newNum++;
        }
    }
    SMAP_LOGGER_INFO("SMAP's managed PID count: %d, pidArr contain new PID count: %d.", oldNum, newNum);
    return (oldNum + newNum) <= GetCurrentMaxNrPid();
}

static inline bool IsCountValid(int count, int max)
{
    return (count > 0 && count <= max);
}

static int CheckPidtype(uint32_t pageType)
{
    int ret = 0;
    char path[PATH_MAX];
    SMAP_LOGGER_INFO("pageType %d.", pageType);
    if (pageType != PAGETYPE_HUGE && pageType != PAGETYPE_NORMAL) {
        SMAP_LOGGER_ERROR("Pagetype is invalid, please input 0 or 1.");
        return -EINVAL;
    }
    ret = snprintf_s(path, sizeof(path), sizeof(path), TIERING_PATH);
    if (ret == -1) {
        return -EINVAL;
    }
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        SMAP_LOGGER_ERROR("cannot find migrate dev under /dev.");
        return -ENODEV;
    }
    ret = ioctl(fd, SMAP_CHECK_PAGESIZE, &pageType);
    if (ret < 0) {
        close(fd);
        SMAP_LOGGER_ERROR("ioctl check page type failed: %d.", ret);
        return -EINVAL;
    }
    close(fd);
    return ret;
}

static bool IsPidTypeValid(int pidType)
{
    struct ProcessManager *pm = GetProcessManager();
    if (!pm) {
        SMAP_LOGGER_ERROR("process manager is null.");
        return false;
    }
    int size = GetNormalPageSize();
    int type = pm->tracking.pageSize == size ? PAGETYPE_NORMAL : PAGETYPE_HUGE;
    return pidType == type;
}

static bool IsLocalNidValid(int nid)
{
    struct ProcessManager *pm = GetProcessManager();
    if (nid >= pm->nrLocalNuma || nid < NUMA_NO_NODE) {
        return false;
    }
    return true;
}

static bool IsNidInNumastat(int nid)
{
    int ret;
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    int nodeExists = 0;
    char nodeStr[DEFAULT_NODE_NUMBER_SIZE];

    if (nid < 0) {
        return false;
    }

    fp = popen("numastat -cvm", "r");
    if (fp == NULL) {
        SMAP_LOGGER_ERROR("Failed to execute numastat.");
        return false;
    }

    ret = snprintf_s(nodeStr, sizeof(nodeStr), (DEFAULT_NODE_NUMBER_SIZE - 1), "Node %d", nid);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("Error: snprintf failed.");
        pclose(fp);
        return false;
    }

    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        if (strstr(line, nodeStr) != NULL) {
            SMAP_LOGGER_INFO("numastat info: %s.", line);
            nodeExists = 1;
            break;
        }
    }

    pclose(fp);

    return nodeExists == 1;
}

static bool IsRemoteNidValid(int nid)
{
    struct ProcessManager *pm = GetProcessManager();
    if (!pm) {
        SMAP_LOGGER_ERROR("process manager is null.");
        return false;
    }
    if (nid < pm->nrLocalNuma || nid >= (REMOTE_NUMA_BITS + pm->nrLocalNuma)) {
        return false;
    }

    return IsNidInNumastat(nid);
}

static bool IsRemoveRemoteNidValid(int nid)
{
    struct ProcessManager *pm = GetProcessManager();
    if (!pm) {
        SMAP_LOGGER_ERROR("process manager is null.");
        return false;
    }
    if (nid < pm->nrLocalNuma || nid >= (REMOTE_NUMA_BITS + pm->nrLocalNuma)) {
        return false;
    }

    return true;
}

static bool IsPidArrValid(pid_t *pidArr, int len, bool ignoreUnmanaged)
{
    int i;
    if (len <= 0 || len > GetCurrentMaxNrPid()) {
        SMAP_LOGGER_ERROR("pidArr invalid len %d.", len);
        return false;
    }
    if (!pidArr) {
        SMAP_LOGGER_ERROR("pidArr is null.");
        return false;
    }
    for (i = 0; i < len; i++) {
        pid_t pid = pidArr[i];
        if (!PidIsValid(pid)) {
            SMAP_LOGGER_ERROR("invalid pid %d.", pid);
            return false;
        }
        if (!ignoreUnmanaged && !GetProcessAttr(pid)) {
            SMAP_LOGGER_ERROR("unmanaged pid %d.", pid);
            return false;
        }
        SMAP_LOGGER_INFO("pid Arr msg num:[%d] pid:%d.", i, pidArr[i]);
    }
    /* 检查pidArr是否有重复项 */
    for (i = 0; i < len - 1; i++) {
        for (int j = i + 1; j < len; j++) {
            if (pidArr[i] == pidArr[j]) {
                SMAP_LOGGER_ERROR("pidArr with duplicate elements: %d.", pidArr[i]);
                return false;
            }
        }
    }
    return true;
}

static int InitVirAPI(void)
{
    int ret;
    ret = OpenVirHandler();
    if (ret) {
        CloseVirHandler();
        SMAP_LOGGER_ERROR("open virsh handler error: %d.", ret);
        return -EBADF;
    }
    return ret;
}

static int InitAllThreads(struct ProcessManager *manager)
{
    int ret;
    EnvMutexLock(&manager->threadLock);
    ret = InitThread(manager, SCAN_MIGRATE_PERIOD, ScanMigrateWork);
    if (ret) {
        SMAP_LOGGER_ERROR("init scan migrate work thread error: %d.", ret);
        DestroyAllThread(manager);
    }
    EnvMutexUnlock(&manager->threadLock);
    return ret;
}

static bool IsNidInArray(int *nidArray, int nidCnt, int targetNid)
{
    for (int i = 0; i < nidCnt; i++) {
        if (nidArray[i] == targetNid) {
            return true;
        }
    }
    return false;
}

// return true if remote nid the [pid] used is different from [nid]
static bool GetNumaInfoFromNumaMaps(char *line, int *nidArray, int nidCnt, pid_t pid,
                                    uint32_t *nodeBitmap, bool hugeFlag)
{
    int i, ret;
    int nrLocalNuma = GetNrLocalNuma();
    char *substr = NULL;
    char pattern[NUMA_MAPS_MAX_PATTERN_LEN];
    /*
     * It's possible that there are multiple Nx= in one line,
     * so it's necessary to traverse all node
     */
    for (i = nrLocalNuma; i < nrLocalNuma + REMOTE_NUMA_NUM; i++) {
        if (IsNidInArray(nidArray, nidCnt, i)) {
            continue;
        }
        ProcessAttr *attr = GetProcessAttrLocked(pid);
        if (attr && InAttrL2(attr, i)) {
            continue;
        }
        ret = snprintf_s(pattern, sizeof(pattern), sizeof(pattern) - 1, "N%d=", i);
        if (ret < 0) {
            SMAP_LOGGER_ERROR("Set pattern failed, pid=%d, ret=%d, i=%d.", pid, ret, i);
            return true;
        }
        substr = strstr(line, pattern);
        if (substr) {
            SMAP_LOGGER_ERROR("Pid %d has N=%d numa maps.", pid, i);
            return true;
        }
    }
    for (i = 0; i < nrLocalNuma; i++) {
        if (hugeFlag && !IsNumaMapLineHuge(line)) {
            continue;
        }
        ret = snprintf_s(pattern, sizeof(pattern), sizeof(pattern) - 1, "N%d=", i);
        if (ret < 0) {
            SMAP_LOGGER_ERROR("Set pattern failed, pid=%d, ret=%d, i=%d.", pid, ret, i);
            return true;
        }
        substr = strstr(line, pattern);
        if (substr) {
            AddL1(nodeBitmap, i);
        }
    }
    return false;
}

static bool IsPidRemoteNidValid(int *nidArray, int nidCnt, pid_t pid, uint32_t *nodeBitmap, int pidType)
{
    bool ret = false;
    FILE *fp;
    char line[MAX_LINE_LENGTH];

    SMAP_LOGGER_INFO("Before open numa_maps");
    fp = OpenNumaMaps(pid);
    if (!fp) {
        SMAP_LOGGER_ERROR("Open pid %d numa maps failed.", pid);
        return false;
    }

    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        ret = GetNumaInfoFromNumaMaps(line, nidArray, nidCnt, pid, nodeBitmap, pidType == VM_TYPE);
        if (ret) {
            SMAP_LOGGER_ERROR("Pid %d nids match numa maps line %s.", pid, line);
            break;
        }
    }
    SMAP_LOGGER_INFO("After fgets numa_maps, pid %d node bitmap: %#x.", pid, *nodeBitmap);

    if (pclose(fp)) {
        SMAP_LOGGER_ERROR("Close numa maps failed, pid=%d.", pid);
    }
    return !ret;
}

static bool IsDestNidVaild(int nid, pid_t pid)
{
    ProcessAttr *attr;
    attr = GetProcessAttrLocked(pid);
    if (!attr) {
        return true;
    }
    if (NotInAttrL2(attr, nid)) {
        return false;
    }
    return true;
}

static bool IsDestNidUnique(struct MigrateOutPayload *payload)
{
    for (int i = 0; i < payload->count; i++) {
        for (int j = i + 1; j < payload->count; j++) {
            if (payload->inner[i].destNid == payload->inner[j].destNid) {
                SMAP_LOGGER_ERROR("pid:%d duplicate destNid found: %d", payload->pid, payload->inner[i].destNid);
                return false;
            }
        }
    }
    return true;
}

static bool CheckMigOutPayloadItems(struct MigrateOutPayload *payload, int *totalRatio)
{
    *totalRatio = 0;
    for (int i = 0; i < payload->count; i++) {
        /* NVMe swap sentinel: skip all NID and param validation */
        if (payload->inner[i].destNid == SMAP_NVME_SWAP_NID) {
            continue;
        }
        if (!IsRemoteNidValid(payload->inner[i].destNid)) {
            SMAP_LOGGER_ERROR("mig para pid:%d destnode%d invalid.", payload->pid, payload->inner[i].destNid);
            return false;
        }
        if (!IsDestNidVaild(payload->inner[i].destNid, payload->pid) && payload->count <= 1) {
            SMAP_LOGGER_ERROR("mig para pid:%d destnode%d conflict.", payload->pid, payload->inner[i].destNid);
            return false;
        }
        if (IsNodeForbidden(payload->inner[i].destNid)) {
            SMAP_LOGGER_ERROR("mig para pid:%d destnode%d forbiddened.", payload->pid, payload->inner[i].destNid);
            return false;
        }
        if (payload->inner[i].migrateMode < MIG_RATIO_MODE || payload->inner[i].migrateMode > MIG_MEMSIZE_MODE) {
            SMAP_LOGGER_ERROR("[%d] pid: %d migrateMode %d invalid.", i, payload->pid, payload->inner[i].migrateMode);
            return false;
        }
        if (GetRunMode() == WATERLINE_MODE && payload->inner[i].migrateMode == MIG_MEMSIZE_MODE) {
            SMAP_LOGGER_ERROR("[%d] smap runMode is WATERLINE_MODE, not supported MIG_MEMSIZE_MODE.", i);
            return false;
        }
        if (GetRunMode() == MEM_POOL_MODE && payload->inner[i].migrateMode != MIG_MEMSIZE_MODE) {
            SMAP_LOGGER_ERROR("[%d] smap runMode is MEM_POOL_MODE, not supported mode except MIG_MEMSIZE_MODE.", i);
            return false;
        }
        if (payload->inner[i].migrateMode == MIG_RATIO_MODE && !IsRatioValid(payload->inner[i].ratio)) {
            SMAP_LOGGER_ERROR("[%d] pid: %d ratio %d invalid.", i, payload->pid, payload->inner[i].ratio);
            return false;
        }
        if (payload->inner[i].migrateMode == MIG_MEMSIZE_MODE && payload->inner[i].memSize % KB_PER_2MB != 0) {
            SMAP_LOGGER_ERROR("[%d] pid: %d memSize %d is not 2M aligned.", i, payload->pid, payload->inner[i].memSize);
            return false;
        }
        if (GetRunMode() == WATERLINE_MODE) {
            *totalRatio += payload->inner[i].ratio;
        }
    }
    return true;
}

static bool IsMigParaValid(struct MigrateOutPayload *payload)
{
    if (!payload) {
        SMAP_LOGGER_ERROR("migrate out payload is null.");
        return false;
    }

    if (!IsDestNidUnique(payload)) {
        SMAP_LOGGER_ERROR("mig para destnode is not unique.");
        return false;
    }

    int totalRatio = 0;
    if (!CheckMigOutPayloadItems(payload, &totalRatio)) {
        return false;
    }

    if (GetRunMode() == WATERLINE_MODE && totalRatio > HUNDRED) {
        SMAP_LOGGER_ERROR("pid %d, migrate out total ration > 100.", payload->pid);
        return false;
    }
    return true;
}

static int CheckMigrateOutMsg(struct MigrateOutMsg *msg, int pidType)
{
    int i;
    if (!msg) {
        SMAP_LOGGER_ERROR("Smap mig out msg is null.");
        return -EINVAL;
    }
    if (!IsPidTypeValid(pidType)) {
        SMAP_LOGGER_ERROR("migrate out pidType %d != current pid type.", pidType);
        return -EINVAL;
    }
    if (!IsCountValid(msg->count, MAX_NR_MIGOUT)) {
        SMAP_LOGGER_ERROR("migrate out count: %d is invalid.", msg->count);
        return -EINVAL;
    }

    for (i = 0; i < msg->count; i++) {
        pid_t currentPid = msg->payload[i].pid;
        for (int j = i + 1; j < msg->count; j++) {
            if (msg->payload[j].pid == currentPid) {
                SMAP_LOGGER_ERROR("migrate out msg exit duplicate pid %d.", msg->payload[i].pid);
                return -EINVAL;
            }
        }
    }

    pid_t uniquePids[MAX_NR_MIGOUT];
    for (i = 0; i < msg->count; i++) {
        uniquePids[i] = msg->payload[i].pid;
    }
    if (!IsMigOutCountValid(uniquePids, msg->count, pidType)) {
        SMAP_LOGGER_ERROR("migrate out count will exceed current max pid count: %d.", GetCurrentMaxNrPid());
        return -EINVAL;
    }

    for (i = 0; i < msg->count; i++) {
        if (msg->payload[i].count <= 0 || msg->payload[i].count > REMOTE_NUMA_NUM) {
            SMAP_LOGGER_ERROR("pid: %d, migrate out payload count:%d is invalid.",
                              msg->payload[i].pid, msg->payload[i].count);
            return false;
        }

        for (int j = 0; j < msg->payload[i].count; j++) {
            SMAP_LOGGER_INFO("mig out msg num:[%d] pid:%d, destNid:%d, ratio:%d, memSize:%llu, migMode:%d.", j,
                msg->payload[i].pid, msg->payload[i].inner[j].destNid, msg->payload[i].inner[j].ratio,
                msg->payload[i].inner[j].memSize, msg->payload[i].inner[j].migrateMode);
        }

        if (!IsMigParaValid(&msg->payload[i])) {
            SMAP_LOGGER_ERROR("mig out msg num:[%d] mig para invalid.", i);
            return -EINVAL;
        }
    }
    return 0;
}

static int ProcessAddTrackingManage(struct MigrateOutMsg *msg, int pidType, uint32_t *nodeBitmap)
{
    int ret = 0;
    if (!msg) {
        SMAP_LOGGER_ERROR("Smap mig out msg is null.");
        return -EINVAL;
    }
    struct AccessAddPidPayload payload[MAX_NR_MIGOUT] = { 0 };
    for (int i = 0; i < msg->count; ++i) {
        payload[i].type = NORMAL_SCAN;
        payload[i].pid = msg->payload[i].pid;
        payload[i].scanTime = LIGHT_STABLE_SCAN_CYCLE;
        if (!PidIsValid(msg->payload[i].pid)) {
            SMAP_LOGGER_WARNING("pid %d doesn't exist.", msg->payload[i].pid);
            payload[i].pid = NON_EXIST_PID;
            continue;
        }
        // assign values for local numa nodes
        if (!nodeBitmap) {
            ret = SetProcessLocalNuma(msg->payload[i].pid, &payload[i].numaNodes, pidType == VM_TYPE);
            if (ret) {
                SMAP_LOGGER_ERROR("Query pid %d memory usage failed: %d.", msg->payload[i].pid, ret);
                return ret;
            }
        } else {
            payload[i].numaNodes = nodeBitmap[i];
        }
        // assign values for remote numa nodes (skip for NVMe swap sentinel)
        for (int j = 0; j < msg->payload[i].count; ++j) {
            if (msg->payload[i].inner[j].destNid == SMAP_NVME_SWAP_NID) {
                SMAP_LOGGER_INFO("pid %d NVMe swap mode: skip AddL2.", payload[i].pid);
                continue;
            }
            SMAP_LOGGER_INFO("Add pid %d migrate dest node to %d.", payload[i].pid, msg->payload[i].inner[j].destNid);
            AddL2ByNid(&payload[i].numaNodes, msg->payload[i].inner[j].destNid);
        }
        SMAP_LOGGER_INFO("pid %d numaNodes %#x.", payload[i].pid, payload[i].numaNodes);
        SMAP_LOGGER_INFO("Set pid %d scan time to %u.", payload[i].pid, payload[i].scanTime);
    }
    ret = AccessIoctlAddPid(msg->count, payload);
    if (ret) {
        SMAP_LOGGER_ERROR("access module add pids error: %d.", ret);
    }
    return ret;
}

static int AddProcessesToGlobalManager(struct MigrateOutMsg *msg, int pidType,
                                       uint32_t *nodeBitmap, bool *hasInvalidPid)
{
    int ret = 0;
    uint32_t *nodeBitmapTmp;
    for (int i = 0; i < msg->count; ++i) {
        nodeBitmapTmp = nodeBitmap ? &nodeBitmap[i] : NULL;
        ProcessParam param = { 0 };
        param.pid = msg->payload[i].pid;
        param.scanTime = pidType == VM_TYPE ? SCAN_TIME_2M : SCAN_TIME_4K;
        param.scanType = NORMAL_SCAN;
        param.count = msg->payload[i].count;

        for (int j = 0; j < msg->payload[i].count; ++j) {
            param.numaParam[j].nid = msg->payload[i].inner[j].destNid;
            param.numaParam[j].ratio = msg->payload[i].inner[j].ratio;
            param.numaParam[j].memSize = msg->payload[i].inner[j].memSize;
            param.numaParam[j].migrateMode = msg->payload[i].inner[j].migrateMode;
        }

        ret = ProcessAddManage(&param, nodeBitmapTmp);
        if (ret) {
            SMAP_LOGGER_ERROR("add process %d failed: %d.", msg->payload[i].pid, ret);
            if (ret == -ESRCH) {
                *hasInvalidPid = true;
                ret = 0;
                continue;
            }
            return ret;
        }
        SMAP_LOGGER_INFO("add process %d done.", param.pid);
    }
    return ret;
}

static int AddProcessNumaBitMap(struct MigrateOutMsg *msg, uint32_t *nodeBitmap, int pidType)
{
    for (int i = 0; i < msg->count; ++i) {
        if (msg->payload[i].count == 0) {
            continue;
        }
        /* NVMe swap mode: skip remote NID validation, only set local NUMA bitmap */
        bool is_nvme_swap = (msg->payload[i].count == 1 &&
                             msg->payload[i].inner[0].destNid == SMAP_NVME_SWAP_NID);
        if (is_nvme_swap) {
            int ret = SetLocalNumaByCpu(msg->payload[i].pid, &nodeBitmap[i]);
            if (ret) {
                SMAP_LOGGER_WARNING("Set pid %d local numa by cpu failed: %d.", msg->payload[i].pid, ret);
            }
            if ((nodeBitmap[i] & LOCAL_NUMA_BIT_MAP_MASK) == 0) {
                nodeBitmap[i] |= LOCAL_NUMA_BIT_MAP_MASK;
            }
            SMAP_LOGGER_INFO("pid:%d NVMe swap mode, local numa bitmap: %#x.", msg->payload[i].pid, nodeBitmap[i]);
            continue;
        }
        int *nidArray = calloc(msg->payload[i].count, sizeof(int));
        if (!nidArray) {
            return -ENOMEM;
        }
        for (int j = 0; j < msg->payload[i].count; ++j) {
            nidArray[j] = msg->payload[i].inner[j].destNid;
        }
        if (!IsPidRemoteNidValid(nidArray, msg->payload[i].count, msg->payload[i].pid, &nodeBitmap[i], pidType)) {
            SMAP_LOGGER_ERROR("Pid %d remote nid conflict.", msg->payload[i].pid);
            free(nidArray);
            return -EINVAL;
        }
        int ret = SetLocalNumaByCpu(msg->payload[i].pid, &nodeBitmap[i]);
        if (ret) {
            SMAP_LOGGER_WARNING("Set pid %d local numa by cpu failed: %d.", msg->payload[i].pid, ret);
        }
        // has no local numa, set all local numa 1
        if (nodeBitmap[i] & LOCAL_NUMA_BIT_MAP_MASK == 0) {
            SMAP_LOGGER_WARNING("Set pid %d all local numa.", msg->payload[i].pid);
            nodeBitmap[i] |= LOCAL_NUMA_BIT_MAP_MASK;
        }
        SMAP_LOGGER_INFO("Set pid:%d local numa bitmap: %#x.", msg->payload[i].pid, nodeBitmap[i]);
        free(nidArray);
    }
    return 0;
}

int ubturbo_smap_migrate_out(struct MigrateOutMsg *msg, int pidType)
{
    struct ProcessManager *manager = GetProcessManager();
    bool hasInvalidPid = false;

    SMAP_LOGGER_INFO("Receive ubturbo_smap_migrate_out msg.");
    if (!ubturbo_smap_is_running()) {
        SMAP_LOGGER_ERROR("Smap already stopped, ubturbo_smap_migrate_out failed.");
        return -EPERM;
    }

    EnvMutexLock(&manager->lock);
    int ret = CheckMigrateOutMsg(msg, pidType);
    if (ret) {
        SMAP_LOGGER_ERROR("Migrate out msg check failed, ret: %d.", ret);
        EnvMutexUnlock(&manager->lock);
        return -EINVAL;
    }

    uint32_t nodeBitmap[MAX_NR_MIGOUT] = { 0 };
    ret = AddProcessNumaBitMap(msg, nodeBitmap, pidType);
    if (ret) {
        SMAP_LOGGER_ERROR("Pid remote nid check failed: %d.", ret);
        EnvMutexUnlock(&manager->lock);
        return ret;
    }

    // send ioctl to add pid to access pid list
    ret = ProcessAddTrackingManage(msg, pidType, nodeBitmap);
    if (ret) {
        SMAP_LOGGER_ERROR("Add process tracking failed: %d.", ret);
        EnvMutexUnlock(&manager->lock);
        return ret;
    }

    // add pid to process manager
    ret = AddProcessesToGlobalManager(msg, pidType, nodeBitmap, &hasInvalidPid);
    if (ret) {
        SMAP_LOGGER_ERROR("Add processes to global manager failed: %d.", ret);
    }

    EnvMutexUnlock(&manager->lock);
    return (ret == 0 && hasInvalidPid) ? -ESRCH : ret;
}

static int CheckMigrateBackMsg(struct MigrateBackMsg *msg)
{
    struct ProcessManager *manager = GetProcessManager();

    if (!IsCountValid(msg->count, MAX_NR_MIGBACK)) {
        SMAP_LOGGER_ERROR("migrateback count : %d is invalid.", msg->count);
        return -EINVAL;
    }
    for (int i = 0; i < msg->count; i++) {
        struct MigrateBackPayload *payload = &msg->payload[i];
        int srcNid = payload->srcNid;
        int destNid = payload->destNid;
        if (IsNodeInvalid(srcNid)) {
            SMAP_LOGGER_ERROR("mig back msg num: [%d] srcNode %d invalid.", i, srcNid);
            return -EINVAL;
        }
        if (IsDestNodeInvalid(destNid)) {
            SMAP_LOGGER_ERROR("mig back msg num: [%d] destNode %d invalid.", i, destNid);
            return -EINVAL;
        }
        // 检查待迁回的NUMA上是否有正在搬迁远端的进程
        if (IsRemoteNumaMigrateBackAllowed(srcNid) <= 0) {
            SMAP_LOGGER_ERROR("srcNode %d not allowed to migrate back.", srcNid);
            return -EINVAL;
        }
        // disable之前先检查是否ready了,如果是内存碎片，不检查
        if (GetRunMode() != MEM_POOL_MODE && !CheckReadyMigrateBack(srcNid)) {
            SMAP_LOGGER_ERROR("migrate back error, srcNid %d not ready to migrate back.", srcNid);
            return -EAGAIN;
        }
    }
    return 0;
}

static bool CheckProcessIdle(int nid)
{
    int nrWait = 0;
    while (nrWait < MAX_CHECK_ALREADY_FORBIDDEN_TIME) {
        SMAP_LOGGER_INFO("Wait already forbidden, nid: %d, nrWait: %d.", nid, nrWait);
        if (IsAllL2NodePidInState(PROC_IDLE, nid)) {
            return true;
        }
        EnvMsleep(WAIT_CHECK_ALREADY_FORBIDDEN_PERIOD);
        nrWait++;
    }
    return false;
}

int ubturbo_smap_migrate_back(struct MigrateBackMsg *msg)
{
    SMAP_LOGGER_INFO("Receive ubturbo_smap_migrate_back msg.");
    if (!ubturbo_smap_is_running()) {
        SMAP_LOGGER_ERROR("Smap already stopped, ubturbo_smap_migrate_back failed.");
        return -EPERM;
    }
    if (!msg) {
        SMAP_LOGGER_ERROR("Smap mig Back msg is null.");
        return -EINVAL;
    }
    int ret = CheckMigrateBackMsg(msg);
    if (ret) {
        SMAP_LOGGER_ERROR("Smap check mig back msg err: %d.", ret);
        return ret;
    }
    // save forbidden info
    EnvAtomic tmpForbiddenNodes[MAX_NODES];
    for (int i = 0; i < MAX_NODES; i++) {
        SaveNodeForbidden(tmpForbiddenNodes, MAX_NODES);
    }
    for (int i = 0; i < msg->count; i++) {
        SetNodeForbidden(msg->payload[i].srcNid);
        SMAP_LOGGER_INFO("smap disable node %d because migrate back.", msg->payload[i].srcNid);
        if (!CheckProcessIdle(msg->payload[i].srcNid)) {
            SMAP_LOGGER_ERROR("Smap check migrate idle timeout.");
            RecoverNodeForbidden(tmpForbiddenNodes, MAX_NODES);
            return -EAGAIN;
        }
    }
    SMAP_LOGGER_INFO("migrateback start.");
    ret = IoctlHandler(msg);
    SMAP_LOGGER_INFO("migrateback result: %d.", ret);
    if (ret != 0) {
        // recover forbidden info
        RecoverNodeForbidden(tmpForbiddenNodes, MAX_NODES);
    }
    return ret;
}

static int CheckSmapRemoveMsg(struct RemoveMsg *msg, int pidType)
{
    if (!IsCountValid(msg->count, MAX_NR_REMOVE)) {
        SMAP_LOGGER_ERROR("smap remove msg count : %d is invalid.", msg->count);
        return -EINVAL;
    }
    if (!IsPidTypeValid(pidType)) {
        SMAP_LOGGER_ERROR("smap remove msg pidType %d != current pid type.", pidType);
        return -EINVAL;
    }
    for (int i = 0; i < msg->count; i++) {
        if (msg->payload[i].count <= 0 || msg->payload[i].count > REMOTE_NUMA_NUM) {
            SMAP_LOGGER_ERROR("[%d] smap remove payload nid count%d invalid.", i, msg->payload[i].count);
            return -EINVAL;
        }
        for (int j = 0; j < msg->payload[i].count; j++) {
            if (!IsRemoveRemoteNidValid(msg->payload[i].nid[j])) {
                SMAP_LOGGER_ERROR("[%d] pid:%d remote node%d invalid.",
                    i, msg->payload[i].pid, msg->payload[i].nid[j]);
                return -EINVAL;
            }
        }
    }
    return 0;
}

static int IoctlClearProcessRemoteNuma(struct RemoveMsg *msg)
{
    pid_t pid;
    int l2node;
    int nrLocalNuma = GetNrLocalNuma();
    // 1. pid不存在的，或者远端numa清空的，直接remove
    // 2. pid存在，远端numa没有清空的就只更新numanodes
    for (int i = 0; i < msg->count; i++) {
        pid = msg->payload[i].pid;
        ProcessAttr *attr = GetProcessAttrLocked(pid);
        if (!attr) {
            struct AccessRemovePidPayload payload = { .pid = pid };
            int ret = AccessIoctlRemovePid(1, &payload);
            if (ret) {
                SMAP_LOGGER_ERROR("access ioctl remove pid %d error: %d.", pid, ret);
                return ret;
            }
            continue;
        }

        uint32_t numaNodes = attr->numaAttr.numaNodes;
        for (int j = 0; j < msg->payload[i].count; j++) {
            l2node = msg->payload[i].nid[j];
            ClearNodeBit(&numaNodes, l2node + (LOCAL_NUMA_BITS - nrLocalNuma));
        }
        if (!GetL2Count(numaNodes)) { // 远端numa清空
            struct AccessRemovePidPayload payload = { .pid = pid };
            int ret = AccessIoctlRemovePid(1, &payload);
            if (ret) {
                SMAP_LOGGER_ERROR("access ioctl remove pid %d error: %d.", pid, ret);
                return ret;
            }
        } else {
            struct AccessAddPidPayload payload = { .pid = pid };
            payload.numaNodes = numaNodes;
            payload.scanTime = attr->scanTime;
            payload.duration = attr->duration;
            payload.type = attr->scanType;
            int ret = AccessIoctlAddPid(1, &payload);
            if (ret) {
                SMAP_LOGGER_ERROR("access ioctl add failed: %d.", ret);
                return ret;
            }
        }
    }
    return 0;
}

static void ClearManagedProcessNuma(int nr, struct RemovePayload *payload)
{
    int ret;
    int nrLocalNuma = GetNrLocalNuma();
    struct ProcessManager *manager = GetProcessManager();
    PidType type = GetPidType(manager);
    for (int i = 0; i < nr; i++) {
        ProcessAttr *attr = manager->processes;
        pid_t pid = payload[i].pid;
        while (attr && attr->pid != pid) {
            attr = attr->next;
        }
        if (!attr) {
            SMAP_LOGGER_WARNING("pid: %d, not exist, not need to remove.", pid);
            continue;
        }

        for (int j = 0; j < payload[i].count; j++) {
            int l2Index = payload[i].nid[j] - nrLocalNuma;
            ClearNodeBit(&attr->numaAttr.numaNodes, l2Index + LOCAL_NUMA_BITS);
        }
        attr->remoteNumaCnt = GetL2Count(attr->numaAttr.numaNodes);
        if (attr->remoteNumaCnt == 0) {
            LinkedListRemove(&attr, &manager->processes);
            SMAP_LOGGER_INFO("Remove pid: %d, from managed process.", pid);
            manager->nr[type]--;
        }
        ret = SyncAllProcessConfig();
        if (ret) {
            SMAP_LOGGER_WARNING("Synchronize pid %d config maybe failed: %d.", pid, ret);
        }
    }
}

int ubturbo_smap_remove(struct RemoveMsg *msg, int pidType)
{
    int ret = 0;
    SMAP_LOGGER_INFO("Receive ubturbo_smap_remove msg.");
    if (!ubturbo_smap_is_running()) {
        SMAP_LOGGER_ERROR("Smap already stopped.");
        return -EPERM;
    }
    if (!msg) {
        SMAP_LOGGER_ERROR("Smap remove msg is null.");
        return -EINVAL;
    }
    ret = CheckSmapRemoveMsg(msg, pidType);
    if (ret) {
        SMAP_LOGGER_ERROR("Check smap remove msg failed: %d.", ret);
        return ret;
    }

    struct ProcessManager *manager = GetProcessManager();
    EnvMutexLock(&manager->lock);
    // send ioctl to remove pid
    ret = IoctlClearProcessRemoteNuma(msg);
    if (ret) {
        SMAP_LOGGER_ERROR("Ioctl clear pid remote numa failed: %d.", ret);
        EnvMutexUnlock(&manager->lock);
        return ret;
    }

    ClearManagedProcessNuma(msg->count, msg->payload);
    EnvMutexUnlock(&manager->lock);
    SMAP_LOGGER_INFO("smap remove result: %d.", ret);
    return ret;
}

int ubturbo_smap_node_enable(struct EnableNodeMsg *msg)
{
    struct ProcessManager *pm = GetProcessManager();
    SMAP_LOGGER_INFO("Receive ubturbo_smap_node_enable msg.");
    if (!ubturbo_smap_is_running()) {
        SMAP_LOGGER_ERROR("Smap already stopped, ubturbo_smap_node_enable failed.");
        return -EPERM;
    }
    if (!msg) {
        SMAP_LOGGER_ERROR("Smap enable node msg is null.");
        return -EINVAL;
    }
    if (msg->nid < pm->nrLocalNuma || msg->nid >= MAX_NODES) {
        SMAP_LOGGER_ERROR("Smap enable node nid %d is invalid.", msg->nid);
        return -EINVAL;
    }
    if (msg->enable == ENABLE_NUMA_MIG) {
        ClearNodeForbidden(msg->nid);
        SMAP_LOGGER_INFO("smap enable node %d.", msg->nid);
    } else if (msg->enable == DISABLE_NUMA_MIG) {
        SetNodeForbidden(msg->nid);
        SMAP_LOGGER_INFO("smap disable node %d.", msg->nid);
    } else {
        SMAP_LOGGER_INFO("enable args:%d is invalid.", msg->enable);
        return -EINVAL;
    }

    return 0;
}

static int InitLog(Logfunc extlog)
{
    if (extlog) {
        UpstreamSubscribeLogger(extlog);
    } else if (SmapStartULog(SMAP_LOG_FILE_PATH)) {
        return -EIO;
    }
    return 0;
}

static int SyncProcessToKernel(void)
{
    int ret;
    int i = 0;
    int maxProcessCnt = GetCurrentMaxNrPid();
    struct AccessAddPidPayload *payload = malloc(maxProcessCnt * sizeof(struct AccessAddPidPayload));
    if (!payload) {
        SMAP_LOGGER_ERROR("AccessAddPidPayload malloc failed.");
        return -ENOMEM;
    }
    struct ProcessManager *manager = GetProcessManager();

    EnvMutexLock(&manager->lock);
    for (ProcessAttr *attr = manager->processes; attr && i < maxProcessCnt; attr = attr->next) {
        payload[i].pid = attr->pid;
        payload[i].numaNodes = attr->numaAttr.numaNodes;
        payload[i].scanTime = attr->scanTime;
        payload[i].type = attr->scanType;
        payload[i].duration = attr->duration;
        i++;
    }
    if (i == 0) {
        SMAP_LOGGER_INFO("SyncAllProcessConfig len: %d, no need to change.", i);
        EnvMutexUnlock(&manager->lock);
        free(payload);
        return 0;
    }
    SMAP_LOGGER_INFO("SyncAllProcessConfig ioctl begin, len: %d.", i);
    ret = AccessIoctlAddPid(i, payload);
    if (ret) {
        SMAP_LOGGER_ERROR("SyncAllProcessConfig ioctl failed: %d.", ret);
    }
    EnvMutexUnlock(&manager->lock);
    free(payload);
    return ret;
}

static void RecoverRemoveInvalidProcess(void)
{
    int ret;
    struct ProcessManager *manager = GetProcessManager();
    PidType type = GetPidType(manager);

    EnvMutexLock(&manager->lock);
    for (ProcessAttr *attr = manager->processes; attr;) {
        pid_t pid = attr->pid;
        ProcessAttr *next = attr->next;
        SMAP_LOGGER_INFO("Recover check if pid %d is valid.", pid);
        if (!PidIsValid(pid)) {
            LinkedListRemove(&attr, &manager->processes);
            manager->nr[type]--;
            ret = SyncAllProcessConfig();
            if (ret) {
                SMAP_LOGGER_WARNING("Synchronize pid %d config maybe failed: %d.", pid, ret);
            }
            SMAP_LOGGER_INFO("Recover remove pid %d from managed process.", pid);
        }
        attr = next;
    }
    EnvMutexUnlock(&manager->lock);
}

static void RecoverAllMMapType(void)
{
    int ret;
    struct ProcessManager *manager = GetProcessManager();

    EnvMutexLock(&manager->lock);
    for (ProcessAttr *attr = manager->processes; attr;) {
        ProcessAttr *next = attr->next;
        ret = VMPreprocess(attr->pid, attr);
        if (ret) {
            SMAP_LOGGER_WARNING("Recover process mmaptype failed on pid %d: %d.", attr->pid, ret);
        }
        attr = next;
    }
    EnvMutexUnlock(&manager->lock);
}

static int Recover(void)
{
    int ret = RecoverFromConfig();
    if (ret) {
        SMAP_LOGGER_ERROR("Recover from config failed: %d.", ret);
        return ret;
    }
    SMAP_LOGGER_INFO("Recover from config done.");
    RecoverRemoveInvalidProcess();
    if (IsHugeMode()) {
        RecoverAllMMapType();
    }

    SMAP_LOGGER_INFO("Recover process mmap type done.");
    ret = SyncProcessToKernel();
    if (ret) {
        SMAP_LOGGER_ERROR("Sync process to kernel failed: %d.", ret);
    }
    return ret;
}

int ubturbo_smap_start(uint32_t pageType, Logfunc extlog)
{
    int ret = 0;
    int lockFd;
    if (EnvAtomicCmpAndSwap(SLEEP, RUNNING, &g_status) == RUNNING) {
        SMAP_LOGGER_WARNING("Smap init failed, already initialized.");
        ret = -EPERM;
        return ret;
    }

    ret = InitLog(extlog);
    if (ret) {
        goto EXIT_ENV;
    }

    SMAP_LOGGER_INFO("Log init success.");
    ret = CheckPidtype(pageType);
    if (ret) {
        SMAP_LOGGER_ERROR("check pid type error %d.", ret);
        goto EXIT_LOGGER;
    }

    ret = ProcessManagerInit(pageType);
    if (ret) {
        SMAP_LOGGER_ERROR("Smap init process manager failed, ret = %d.", ret);
        goto EXIT_LOGGER;
    }

    struct ProcessManager *manager = GetProcessManager();
    ret = InitTrackingDev(manager);
    if (ret) {
        SMAP_LOGGER_ERROR("Smap init tracking dev failed, ret = %d.", ret);
        goto EXIT_DEV;
    }

    ret = AccessIoctlRemoveAllPid();
    if (ret) {
        SMAP_LOGGER_ERROR("access ioctl remove all pid error: %d.", ret);
        goto EXIT_DEV;
    }

    if (IsHugeMode()) {
        ret = InitVirAPI();
        if (ret) {
            SMAP_LOGGER_ERROR("Get libvirt API failed, ret = %d.", ret);
            goto EXIT_DEV;
        }
    }

    // Recover only after manager->nrLocalNuma has been set and kernel's pid has been all removed
    ret = Recover();
    if (ret) {
        SMAP_LOGGER_ERROR("Recover config failed: %d.", ret);
        ret = -EBADF;
        goto EXIT_VIR;
    }
    SMAP_LOGGER_INFO("Recover config done.");

    ret = InitAllThreads(manager);
    if (ret) {
        SMAP_LOGGER_ERROR("Smap init threads failed, ret = %d.", ret);
        goto EXIT_VIR;
    }
    SMAP_LOGGER_INFO("Smap init success.");
    return ret;

EXIT_VIR:
    CloseVirHandler();
EXIT_DEV:
    DeinitTrackingDev(manager);
    DestroyProcessManager();
EXIT_LOGGER:
    SmapLoggerExit();
EXIT_ENV:
    EnvAtomicSet(&g_status, SLEEP);
    return ret;
}

int ubturbo_smap_stop(void)
{
    SMAP_LOGGER_INFO("Receive ubturbo_smap_stop msg.");
    if (EnvAtomicCmpAndSwap(RUNNING, SLEEP, &g_status) == SLEEP) {
        SMAP_LOGGER_ERROR("Smap stop failed, already stopped.");
        return -EPERM;
    }

    struct ProcessManager *manager = GetProcessManager();
    EnvMutexLock(&manager->threadLock);
    DestroyAllThread(manager);
    EnvMutexUnlock(&manager->threadLock);
    SMAP_LOGGER_INFO("All threads destroyed.");
    if (IsHugeMode()) {
        CloseVirHandler();
        SMAP_LOGGER_INFO("Libvirt handler closed.");
    }

    RemoveAllManagedProcess();
    SMAP_LOGGER_INFO("All managed processes removed.");
    for (int nid = 0; nid < MAX_NODES; nid++) {
        ClearNodeForbidden(nid);
    }
    SMAP_LOGGER_INFO("All node forbidden state cleared.");

    DeinitTrackingDev(manager);
    SMAP_LOGGER_INFO("Tracking device deinited.");

    DestroyProcessManager();

    SMAP_LOGGER_INFO("Smap stop success.");

    SmapLoggerExit();

    return 0;
}

void ubturbo_smap_urgent_migrate_out(uint64_t size)
{
    SMAP_LOGGER_INFO("Receive ubturbo_smap_urgent_migrate_out msg.");
    if (!ubturbo_smap_is_running()) {
        SMAP_LOGGER_ERROR("Smap is not running.");
        return;
    }

    // 遍历pid看是否有足够的size，有则立即进行迁移
    SMAP_LOGGER_INFO("ubturbo_smap_urgent_migrate_out size = %llu.", size);
    FindPidMigrateSize(size);
    SMAP_LOGGER_INFO("ubturbo_smap_urgent_migrate_out success.");
    return;
}

int ubturbo_smap_remote_numa_info_set(struct SetRemoteNumaInfoMsg *msg)
{
    int ret;
    SMAP_LOGGER_INFO("Receive ubturbo_smap_remote_numa_info_set msg.");
    if (!ubturbo_smap_is_running()) {
        SMAP_LOGGER_ERROR("Smap already stopped.Smap get borrow mem failed.");
        return -EPERM;
    }
    if (!msg) {
        SMAP_LOGGER_ERROR("Set smap remote numa info msg is null.");
        return -EINVAL;
    }
    if (!IsLocalNidValid(msg->srcNid)) {
        SMAP_LOGGER_ERROR("Set smap remote numa info src node%d invalid.", msg->srcNid);
        return -EINVAL;
    }
    if (!IsRemoteNidValid(msg->destNid)) {
        SMAP_LOGGER_ERROR("Set smap remote numa info dest node%d invalid.", msg->destNid);
        return -EINVAL;
    }
    if (msg->size > REMOTE_NUMA_MEMORY_MAX) {
        SMAP_LOGGER_ERROR("Set smap remote numa info size %llu invalid.", msg->size);
        return -EINVAL;
    }
    ret = SetRemoteNumaInfo(msg->srcNid, msg->destNid, msg->size);
    if (ret) {
        SMAP_LOGGER_ERROR("Set smap remote numa info failed: %d.", ret);
        return ret;
    }
    SMAP_LOGGER_INFO("Set smap remote numa info %d-%d to %llu.", msg->srcNid, msg->destNid, msg->size);
    return 0;
}

static int CheckQueryVMFreqMsgValid(int pid, uint16_t *data, uint32_t lengthIn, uint32_t *lengthOut, int dataSource)
{
    time_t currentTime;
    struct ProcessManager *manager = GetProcessManager();
    if (!data || !lengthOut) {
        SMAP_LOGGER_ERROR("data or lengthOut null, ubturbo_smap_freq_query failed.");
        return -EINVAL;
    }
    if (lengthIn == 0) {
        SMAP_LOGGER_ERROR("lengthIn(%llu) invalid. ubturbo_smap_freq_query failed.", lengthIn);
        return -EINVAL;
    }
    if (dataSource < 0 || dataSource >= MAX_SOURCE) {
        SMAP_LOGGER_ERROR("dataSource(%d) invalid, limit(%d). ubturbo_smap_freq_query failed.", dataSource,
                          MAX_SOURCE);
        return -EINVAL;
    }

    ProcessAttr *attr = GetProcessAttr(pid);
    if (!attr) {
        SMAP_LOGGER_ERROR("pid %d is not in managed process list\n", pid);
        return -EINVAL;
    }
    if (dataSource == STATISTIC_DATA_SOURCE && attr->scanType != STATISTIC_SCAN) {
        SMAP_LOGGER_ERROR("pid %d is not in statistic mode\n", pid);
        return -EINVAL;
    }
    if (dataSource == NORMAL_DATA_SOURCE && attr->scanType != NORMAL_SCAN) {
        SMAP_LOGGER_ERROR("pid %d is not in normal mode\n", pid);
        return -EINVAL;
    }
    if (time(&currentTime) == (time_t)-1) {
        SMAP_LOGGER_ERROR("get time error");
    }
    SMAP_LOGGER_INFO("Current time: %s\n", ctime(&currentTime));
    if (dataSource == STATISTIC_DATA_SOURCE && (currentTime - attr->scanStart < attr->duration)) {
        SMAP_LOGGER_ERROR("pid %d scan duaration did not meet the expected threshold\n", pid);
        return -EAGAIN;
    }
    return 0;
}

static int QueryVMFreqFromKernel(int pid, uint16_t *data, uint32_t lengthIn, uint32_t *lengthOut)
{
    int ret;
    struct ProcessManager *manager = GetProcessManager();
    struct TrakingInfoPayload payload = {
        .pid = pid,
        .length = lengthIn,
        .data = data,
    };
    ret = ioctl(manager->fds.access, SMAP_ACCESS_GET_TRACKING, &payload);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("access ioctl remove get tracking info error: %s\n", strerror(errno));
        return -EBADF;
    }
    *lengthOut = payload.length;
    return ret;
}

static int QueryVMFreqFromUser(int pid, uint16_t *data, uint32_t lengthIn, uint32_t *lengthOut)
{
    uint64_t i = 0;
    uint64_t actcLen = 0;
    struct ProcessManager *manager = GetProcessManager();
    EnvMutexLock(&manager->lock);
    ProcessAttr *attr = GetProcessAttrLocked(pid);
    if (!attr) {
        SMAP_LOGGER_ERROR("pid %d doesn't exist, ubturbo_smap_freq_query failed.", pid);
        EnvMutexUnlock(&manager->lock);
        return -EINVAL;
    }
    for (int nid = 0; nid < MAX_NODES; nid++) {
        actcLen += attr->scanAttr.actcLen[nid];
    }
    if (actcLen == 0) {
        *lengthOut = 0;
        SMAP_LOGGER_ERROR("pid %d, actcLen %llu, ubturbo_smap_freq_query failed.", pid, actcLen);
        EnvMutexUnlock(&manager->lock);
        return -EINVAL;
    }
    *lengthOut = (actcLen > lengthIn) ? lengthIn : actcLen;
    for (int nid = 0; nid < MAX_NODES; nid++) {
        ActcData *actc = attr->scanAttr.actcData[nid];
        actcLen = attr->scanAttr.actcLen[nid];
        for (uint64_t j = 0; j < actcLen && i < lengthIn; i++, j++) {
            data[i] = actc[j].freq;
        }
    }
    SMAP_LOGGER_INFO("ubturbo_smap_freq_query success, pid %d lengthIn %llu lengthOut %llu.", pid, lengthIn,
                     *lengthOut);
    EnvMutexUnlock(&manager->lock);
    return 0;
}

int ubturbo_smap_freq_query(int pid, uint16_t *data, uint32_t lengthIn, uint32_t *lengthOut, int dataSource)
{
    SMAP_LOGGER_INFO("Receive ubturbo_smap_freq_query msg, dataSource: %d.\n", dataSource);
    if (!ubturbo_smap_is_running()) {
        SMAP_LOGGER_ERROR("Smap isn't running. ubturbo_smap_freq_query failed.\n");
        return -EPERM;
    }
    int ret = CheckQueryVMFreqMsgValid(pid, data, lengthIn, lengthOut, dataSource);
    if (ret) {
        SMAP_LOGGER_ERROR("ubturbo_smap_freq_query check msg valid failed: %d\n", ret);
        return ret;
    }

    if (dataSource == STATISTIC_DATA_SOURCE) {
        ret = QueryVMFreqFromKernel(pid, data, lengthIn, lengthOut);
    } else {
        ret = QueryVMFreqFromUser(pid, data, lengthIn, lengthOut);
    }
    if (ret) {
        SMAP_LOGGER_ERROR("ubturbo_smap_freq_query failed: %d\n", ret);
        return ret;
    }
    SMAP_LOGGER_INFO("ubturbo_smap_freq_query success, pid %d lengthIn %llu lengthOut %llu.\n", pid, lengthIn,
                     *lengthOut);
    return 0;
}

// 根据使用场景配置SMAP的运行模式
int ubturbo_smap_run_mode_set(int runMode)
{
    int ret;
    SMAP_LOGGER_INFO("Receive ubturbo_smap_run_mode_set msg, runMode:%d.", runMode);
    if (!ubturbo_smap_is_running()) {
        SMAP_LOGGER_ERROR("Smap isn't running, set runMode failed.");
        return -EPERM;
    }

    if (runMode < 0 || runMode >= MAX_RUN_MODE) {
        SMAP_LOGGER_ERROR("runMode(%d) invalid, Smap set run mode failed.", runMode);
        return -EINVAL;
    }

    int pageSize = GetPageSize();
    if (runMode == MEM_POOL_MODE && pageSize != GetHugePageSize()) {
        SMAP_LOGGER_ERROR("Not Huge Page mode, set run mode failed.");
        return -EINVAL;
    }

    ret = SyncRunMode(runMode);
    if (ret) {
        SMAP_LOGGER_ERROR("Sync run mode %d to config failed: %d.", runMode, ret);
        return -EBADF;
    }
    SetRunMode(runMode);
    if (runMode == WATERLINE_MODE) {
        SetAdaptMem(true);
    }
    if (runMode == MEM_POOL_MODE) {
        SetAdaptMem(false);
    }
    SMAP_LOGGER_INFO("runMode(%d) set success.", runMode);
    return 0;
}

static int CheckAddProcessTrackingMsg(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType)
{
    struct ProcessManager *pm = GetProcessManager();
    ProcessAttr *current = pm->processes;
    if (pidArr == NULL || scanTime == NULL || duration == NULL) {
        SMAP_LOGGER_ERROR("Smap check add process tracking pid list or scan time list is NULL.");
        return -EINVAL;
    }
    if (len <= 0 || len > MAX_NR_MIGOUT) {
        SMAP_LOGGER_ERROR("Smap check add process tracking pidArr len is invalid.");
        return -EINVAL;
    }
    int pidType = IsHugeMode() ? PAGETYPE_HUGE : PAGETYPE_NORMAL;
    if (!IsMigOutCountValid(pidArr, len, pidType)) {
        SMAP_LOGGER_ERROR("Smap add process tracking len %d is invalid.", len);
        return -EINVAL;
    }
    while (current) {
        for (int i = 0; i < len; i++) {
            if (current->pid == pidArr[i] && current->state != PROC_MOVE) {
                SMAP_LOGGER_ERROR("The pid %d state is %d, not PROC_MOVE.", pidArr[i], current->state);
                return -EINVAL;
            }
        }
        current = current->next;
    }
    for (int i = 0; i < len; i++) {
        if (!PidIsValid(pidArr[i])) {
            SMAP_LOGGER_ERROR("The pid %d is invaild.", pidArr[i]);
            return -EINVAL;
        }
        if ((scanTime[i] > MAX_SCAN_TIME) || (scanTime[i] < MIN_SCAN_TIME) || (scanTime[i] % MIN_SCAN_TIME != 0)) {
            SMAP_LOGGER_ERROR("The scan time %d is invaild.", scanTime[i]);
            return -EINVAL;
        }
        if (duration[i] > MAX_SCAN_DURATION_SEC || duration[i] == 0) {
            SMAP_LOGGER_ERROR("The scan duration %u is invaild.", duration[i]);
            return -EINVAL;
        }
    }
    if (scanType >= SCAN_TYPE_MAX || scanType < 0) {
        SMAP_LOGGER_ERROR("The scan type %d is invaild.", scanType);
        return -EINVAL;
    }
    return 0;
}

static int AddProcessTracking(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType)
{
    int ret = 0;
    struct ProcessManager *pm = GetProcessManager();
    int maxProcessCnt = GetCurrentMaxNrPid();
    struct AccessAddPidPayload *payload = calloc(maxProcessCnt, sizeof(struct AccessAddPidPayload));
    if (!payload) {
        SMAP_LOGGER_ERROR("AccessAddPidPayload malloc failed.");
        return -ENOMEM;
    }
    for (int i = 0; i < len; i++) {
        ret = SetProcessLocalNuma(pidArr[i], &payload[i].numaNodes, IsHugeMode());
        if (ret) {
            SMAP_LOGGER_ERROR("Query pid %d memory usage failed: %d.", pidArr[i], ret);
            free(payload);
            return ret;
        }
        payload[i].pid = pidArr[i];
        payload[i].scanTime = scanTime[i];
        payload[i].duration = duration[i];
        payload[i].type = scanType;
        ProcessAttr *attr = GetProcessAttrLocked(pidArr[i]);
        if (scanType == NORMAL_SCAN) {
            if (!attr) {
                SMAP_LOGGER_ERROR("pid %d is not managed , scan type can not be %d.", pidArr[i], scanType);
                free(payload);
                return -EINVAL;
            }
            if (!IsRemoteNidValid(GetAttrL2(attr))) {
                SMAP_LOGGER_ERROR("pid %d l2 node is invalid, scan type is %d.", attr->pid, scanType);
                free(payload);
                return -EINVAL;
            }
            SetL2ByNid(&payload[i].numaNodes, GetAttrL2(attr));
        } else {
            if (attr == NULL || GetAttrL2(attr) == DEFAULT_L2_NODE) {
                ClearL2(&payload[i].numaNodes);
            } else {
                SetL2ByNid(&payload[i].numaNodes, GetAttrL2(attr));
            }
        }
    }
    ret = AccessIoctlAddPid(len, payload);
    if (ret) {
        SMAP_LOGGER_ERROR("access module add pids error: %d.", ret);
    }
    free(payload);
    return ret;
}

int ubturbo_smap_process_tracking_add(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType)
{
    int ret = 0;
    struct ProcessManager *manager = GetProcessManager();
    SMAP_LOGGER_INFO("Receive ubturbo_smap_process_tracking_add msg, len:%d, scanType:%d.", len, scanType);
    if (EnvAtomicRead(&g_status) != RUNNING) {
        SMAP_LOGGER_ERROR("Smap isn't running, add process tracking failed.");
        return -EPERM;
    }
    if (!IsPidTypeValid(PAGETYPE_HUGE) && scanType != STATISTIC_SCAN) {
        SMAP_LOGGER_ERROR("Smap Add Process Tracking pid type invalid, expected %d.", PAGETYPE_HUGE);
        return -EINVAL;
    }
    EnvMutexLock(&manager->lock);
    ret = CheckAddProcessTrackingMsg(pidArr, scanTime, duration, len, scanType);
    if (ret) {
        SMAP_LOGGER_ERROR("Smap check add process tracking msg failed.");
        EnvMutexUnlock(&manager->lock);
        return ret;
    }

    ret = AddProcessTracking(pidArr, scanTime, duration, len, scanType);
    if (ret) {
        SMAP_LOGGER_ERROR("Smap module add process tracking failed.");
        EnvMutexUnlock(&manager->lock);
        return ret;
    }
    SMAP_LOGGER_INFO("Add process tracking done.");
    // add pid to process manager
    for (int i = 0; i < len; i++) {
        ProcessParam param = { 0 };
        param.count = 1;
        param.pid = pidArr[i];
        ProcessAttr *attr = GetProcessAttrLocked(pidArr[i]);
        if (attr) {
            param.numaParam[0].ratio = HUNDRED - attr->initLocalMemRatio;
            param.numaParam[0].nid = GetAttrL2(attr);
        } else {
            param.numaParam[0].ratio = 0;
            param.numaParam[0].nid = DEFAULT_L2_NODE;
        }
        param.scanTime = scanTime[i];
        param.duration = duration[i];
        param.scanType = scanType;
        param.numaParam[0].migrateMode = MIG_RATIO_MODE;
        ret = ProcessAddManage(&param, NULL);
        if (ret) {
            SMAP_LOGGER_ERROR("Add process tracking %d failed: %d.", pidArr[i], ret);
            EnvMutexUnlock(&manager->lock);
            return ret;
        }
        SMAP_LOGGER_INFO("Add process %d tracking done.", pidArr[i]);
    }
    SMAP_LOGGER_INFO("Add process tracking manage result: %d.", ret);
    EnvMutexUnlock(&manager->lock);
    return ret;
}

static int CheckRemoveProcessTrackingMsg(pid_t *pidArr, int len)
{
    int ret = 0;
    struct ProcessManager *manager = GetProcessManager();

    if (pidArr == NULL) {
        SMAP_LOGGER_ERROR("Smap check remove process tracking pid list  is NULL.");
        return -EINVAL;
    }
    if (len <= 0 || len > MAX_NR_REMOVE) {
        SMAP_LOGGER_ERROR("Smap remove tracking info len is invalid, which is %d.", len);
        return -EINVAL;
    }
    for (int i = 0; i < len; i++) {
        ProcessAttr *attr = manager->processes;
        while (attr && attr->pid != pidArr[i]) {
            attr = attr->next;
        }
        if (!attr) {
            continue;
        }
        if (attr->scanType == NORMAL_SCAN || attr->state == PROC_MIGRATE || attr->state == PROC_BACK) {
            ret = -EINVAL;
            SMAP_LOGGER_ERROR("Pid %d scan type is %d, state is %d.", attr->pid, attr->scanType, attr->state);
            break;
        }
    }
    return ret;
}

int ubturbo_smap_process_tracking_remove(pid_t *pidArr, int len, int flag)
{
    int ret = 0;
    SMAP_LOGGER_INFO("Receive ubturbo_smap_process_tracking_remove msg, len:%d, flag:%d.", len, flag);
    if (EnvAtomicRead(&g_status) != RUNNING) {
        SMAP_LOGGER_ERROR("Smap isn't running, remove process tracking failed.");
        return -EPERM;
    }
    struct ProcessManager *manager = GetProcessManager();
    int maxProcessCnt = GetCurrentMaxNrPid();
    EnvMutexLock(&manager->lock);
    ret = CheckRemoveProcessTrackingMsg(pidArr, len);
    if (ret) {
        SMAP_LOGGER_ERROR("Smap check remove process tracking msg failed, ret is %d.", ret);
        EnvMutexUnlock(&manager->lock);
        return ret;
    }
    SMAP_LOGGER_INFO("Check ubturbo_smap_process_tracking_remove msg done.");
    struct AccessRemovePidPayload *payload = malloc(maxProcessCnt * sizeof(struct AccessRemovePidPayload));
    if (!payload) {
        SMAP_LOGGER_ERROR("AccessRemovePidPayload malloc failed.");
        EnvMutexUnlock(&manager->lock);
        return -ENOMEM;
    }
    for (int i = 0; i < len; i++) {
        payload[i].pid = pidArr[i];
    }
    ret = AccessIoctlRemovePid(len, payload);
    free(payload);
    if (ret) {
        SMAP_LOGGER_WARNING("Process tracking access ioctl remove pid error: %d.", ret);
        EnvMutexUnlock(&manager->lock);
        return ret;
    }
    SMAP_LOGGER_INFO("Remove process from kernel done.");

    RemoveManagedProcess(len, pidArr);
    EnvMutexUnlock(&manager->lock);
    SMAP_LOGGER_INFO("Remove process tracking manage result: %d.", ret);
    return ret;
}

int ubturbo_smap_process_migrate_enable(pid_t *pidArr, int len, int enable, int flags)
{
    SMAP_LOGGER_INFO("Receive ubturbo_smap_process_migrate_enable msg, len:%d, enable:%d, flags:%d.", len, enable,
                     flags);
    if (EnvAtomicRead(&g_status) != RUNNING) {
        SMAP_LOGGER_ERROR("ubturbo_smap_process_migrate_enable smap isn't running.");
        return -EPERM;
    }
    if (enable != DISABLE_PROCESS_MIGRATE && enable != ENABLE_PROCESS_MIGRATE) {
        SMAP_LOGGER_ERROR("ubturbo_smap_process_migrate_enable invalid enable %d.", enable);
        return -EINVAL;
    }
    if (!IsPidArrValid(pidArr, len, true)) {
        SMAP_LOGGER_ERROR("ubturbo_smap_process_migrate_enable not all pid is valid.");
        return -EINVAL;
    }
    SMAP_LOGGER_DEBUG("ubturbo_smap_process_migrate_enable parameters are all valid.");

    return EnableProcessMigrate(pidArr, len, enable);
}

static int CheckMigrateNumaMsg(struct MigrateNumaMsg *msg)
{
    struct ProcessManager *manager = GetProcessManager();

    if (!msg) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate msg is null.");
        return -EINVAL;
    }
    if (msg->srcNid == msg->destNid) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate srcNid = destNid.");
        return -EINVAL;
    }
    if (!IsRemoteNidValid(msg->srcNid)) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate invalid srcNid %d.", msg->srcNid);
        return -EINVAL;
    }
    if (!IsRemoteNidValid(msg->destNid)) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate invalid destNid %d.", msg->destNid);
        return -EINVAL;
    }
    if (msg->count <= 0 || msg->count > MAX_NR_MIGNUMA) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate invalid count %d.", msg->count);
        return -EINVAL;
    }
    // 检查srcNid和destNid上的pid是否都已停止迁移
    if (IsRemoteNumaMoveAllowed(msg->srcNid) <= 0) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate srcNid %d not allowed to move.", msg->srcNid);
        return -EINVAL;
    }
    if (IsRemoteNumaMoveAllowed(msg->destNid) <= 0) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate destNid %d not allowed to move.", msg->destNid);
        return -EINVAL;
    }
    return 0;
}

int ubturbo_smap_remote_numa_migrate(struct MigrateNumaMsg *msg)
{
    struct ProcessManager *manager = GetProcessManager();
    SMAP_LOGGER_INFO("Receive ubturbo_smap_remote_numa_migrate msg.");
    if (EnvAtomicRead(&g_status) != RUNNING) {
        SMAP_LOGGER_ERROR("smap isn't running, migrate remote numa failed.");
        return -EPERM;
    }
    if (CheckMigrateNumaMsg(msg)) {
        return -EINVAL;
    }
    SMAP_LOGGER_DEBUG("ubturbo_smap_remote_numa_migrate parameters are all valid.");

    int ret = MigrateRemoteNuma(manager, (struct MigrateNumaIoctlMsg *)msg);
    if (ret) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate migrate remote numa failed: %d.", ret);
        return ret;
    }
    ret = ChangePidRemoteByNuma(msg->srcNid, msg->destNid);
    SMAP_LOGGER_INFO("ubturbo_smap_remote_numa_migrate change pid remote ret: %d.", ret);
    return ret;
}

static int CheckSameMigrateNumaMsg(struct MigrateNumaMsg *msg)
{
    struct ProcessManager *manager = GetProcessManager();

    if (!msg) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate msg is null.");
        return -EINVAL;
    }
    if (msg->srcNid != msg->destNid) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate srcNid = destNid.");
        return -EINVAL;
    }
    if (!IsRemoteNidValid(msg->srcNid)) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate invalid srcNid %d.", msg->srcNid);
        return -EINVAL;
    }
    if (msg->count <= 0 || msg->count > MAX_NR_MIGNUMA) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate invalid count %d.", msg->count);
        return -EINVAL;
    }
    // 检查Nid上的pid是否都已停止迁移
    if (IsRemoteNumaMoveAllowed(msg->srcNid) <= 0) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_migrate srcNid %d not allowed to move.", msg->srcNid);
        return -EINVAL;
    }
    return 0;
}

int ubturbo_smap_same_remote_numa_migrate(struct MigrateNumaMsg *msg)
{
    struct ProcessManager *manager = GetProcessManager();
    SMAP_LOGGER_INFO("Receive ubturbo_smap_same_remote_numa_migrate msg.");
    if (EnvAtomicRead(&g_status) != RUNNING) {
        SMAP_LOGGER_ERROR("smap isn't running, migrate remote numa failed.");
        return -EPERM;
    }
    if (CheckSameMigrateNumaMsg(msg)) {
        return -EINVAL;
    }
    SMAP_LOGGER_DEBUG("ubturbo_smap_same_remote_numa_migrate parameters are all valid.");

    int ret = MigrateRemoteNuma(manager, (struct MigrateNumaIoctlMsg *)msg);
    if (ret) {
        SMAP_LOGGER_ERROR("ubturbo_smap_same_remote_numa_migrate migrate remote numa failed: %d.", ret);
        return ret;
    }
    ret = ChangePidRemoteByNuma(msg->srcNid, msg->destNid);
    SMAP_LOGGER_INFO("ubturbo_smap_same_remote_numa_migrate change pid remote ret: %d.", ret);
    return ret;
}

static int CheckMigOutSyncMsg(int pidType, uint64_t maxWaitTime)
{
    SMAP_LOGGER_INFO("received ubturbo_smap_migrate_out_sync msg, maxWaitTime:%llu.", maxWaitTime);
    if ((maxWaitTime < MIN_WAIT_TIME || maxWaitTime > MAX_WAIT_TIME) && maxWaitTime != 0) {
        SMAP_LOGGER_ERROR("The maxWaitTime parameter is improper,The maxWaitTime from 10s to 1 min.");
        return -EINVAL;
    }

    if (GetRunMode() != MEM_POOL_MODE) {
        SMAP_LOGGER_ERROR("Smap is not MEM_POOL_MODE, ubturbo_smap_migrate_out_sync failed.");
        return -EINVAL;
    }

    if (pidType != PAGETYPE_HUGE) {
        SMAP_LOGGER_ERROR("pidType is not 2M, ubturbo_smap_migrate_out_sync failed.");
        return -EINVAL;
    }

    return 0;
}

static int CheckMigOutSyncResult(struct MigrateOutMsg *msg, int *invalidPidNum, bool *allPidSuccess,
    uint64_t maxWaitTime)
{
    int num = 0;
    bool result = true;
    struct ProcessManager *manager = GetProcessManager();
    ProcessAttr *attr;

    for (int i = 0; i < msg->count; i++) {
        bool isMultiNumaPid = false;
        EnvMutexLock(&manager->lock);
        attr = manager->processes;
        while (attr && attr->pid != msg->payload[i].pid) {
            attr = attr->next;
        }
        if (!attr) {
            num++;
            EnvMutexUnlock(&manager->lock);
            SMAP_LOGGER_ERROR("Pid %d is invalid.", msg->payload[i].pid);
            continue;
        }
        bool isSuccess = MigOutIsDone(attr, &isMultiNumaPid);
        EnvMutexUnlock(&manager->lock);
        result &= isSuccess;
        if (!isSuccess && IsMemoryLow(msg->payload[i].pid) && maxWaitTime == 0) {
            SMAP_LOGGER_ERROR("Dest numa memory is not enough.");
            return -EBUSY;
        }
        if (maxWaitTime == 0 && !isMultiNumaPid) {
            SMAP_LOGGER_ERROR("Pid %d is single numa pid or unmanaged pid.", msg->payload[i].pid);
            return -EINVAL;
        }
    }
    *invalidPidNum = num;
    *allPidSuccess = result;
    return 0;
}

int ubturbo_smap_migrate_out_sync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime)
{
    int ret;
    uint64_t waitTime = 0;
    bool allPidSuccess = true;
    int invalidPidNum;

    ret = CheckMigOutSyncMsg(pidType, maxWaitTime);
    if (ret) {
        return ret;
    }

    ret = ubturbo_smap_migrate_out(msg, pidType);
    if (ret && ret != -ESRCH) {
        SMAP_LOGGER_ERROR("Smap migrate out failed, ret %d.", ret);
        return ret;
    }
    SMAP_LOGGER_INFO("Smap migrate out done.");

    while (maxWaitTime == 0 || waitTime < maxWaitTime) {
        waitTime += WAIT_TIME;
        invalidPidNum = 0;
        SMAP_LOGGER_INFO("ubturbo_smap_migrate_out_sync waitTime %llu.", waitTime);
        ret = CheckMigOutSyncResult(msg, &invalidPidNum, &allPidSuccess, maxWaitTime);
        if (ret) {
            return ret;
        }
        if (invalidPidNum == msg->count) {
            SMAP_LOGGER_ERROR("ubturbo_smap_migrate_out_sync all pid is invalid.");
            return -ESRCH;
        }
        if (allPidSuccess && !invalidPidNum) {
            SMAP_LOGGER_INFO("ubturbo_smap_migrate_out_sync all pid success.");
            return 0;
        } else if (allPidSuccess && invalidPidNum) {
            SMAP_LOGGER_INFO("ubturbo_smap_migrate_out_sync partial pid success.");
            return -ESRCH;
        }
        allPidSuccess = true;
        EnvMsleep(WAIT_TIME);
    }
    SMAP_LOGGER_ERROR("Migration timed out. pidType %d, ret %d.", pidType, ret);
    return -EBUSY;
}

static bool IsScanTypeValid(pid_t *pidArr, int len)
{
    int i;
    ProcessAttr *attr;

    for (i = 0; i < len; i++) {
        pid_t pid = pidArr[i];
        attr = GetProcessAttr(pid);
        if (!attr) {
            SMAP_LOGGER_ERROR("get process attr failed, pid:%d.", pid);
            return false;
        }
        if (attr->scanType != NORMAL_SCAN) {
            SMAP_LOGGER_ERROR("pid %d invalid scan type %d.", pid, attr->scanType);
            return false;
        }
    }
    return true;
}

/*
 * 检查pidArr的远端NUMA是否都是nid
 *
 * 返回值：0-是，其它-异常
 */
static int IsPidArrRemoteNumaMatch(struct MigrateEscapeMsg *msg)
{
    struct ProcessManager *manager = GetProcessManager();
    int ret = 0;
    EnvMutexLock(&manager->lock);
    for (int i = 0; i < msg->count; i++) {
        ProcessAttr *attr = GetProcessAttrLocked(msg->payload[i].pid);
        if (!attr) {
            SMAP_LOGGER_ERROR("GetProcessAttrLocked pid %d null.", msg->payload[i].pid);
            ret = -EINVAL;
            break;
        }
        if (NotInAttrL2(attr, msg->payload[i].srcNid)) {
            SMAP_LOGGER_ERROR("pid %d remote nid is not %d", msg->payload[i].pid, msg->payload[i].srcNid);
            ret = -ENXIO;
            break;
        }
    }
    EnvMutexUnlock(&manager->lock);
    return ret;
}

static int GetAttrNidInitRatio(pid_t pid, int nid)
{
    int nrLocalNuma = GetNrLocalNuma();
    ProcessAttr *attr = GetProcessAttr(pid);
    if (!attr) {
        return -EINVAL;
    }
    int l1node = GetAttrL1(attr);
    int curRatio = attr->strategyAttr.initRemoteMemRatio[l1node][nid - nrLocalNuma];
    return attr->strategyAttr.initRemoteMemRatio[l1node][nid - nrLocalNuma];
}

static bool IsRemoteNidRatioValid(pid_t pid, int nid, int ratio)
{
    if (ratio <= 0 || ratio > HUNDRED) {
        return false;
    }
    int curRatio = GetAttrNidInitRatio(pid, nid);
    if (curRatio < 0) {
        return false;
    }
    if (ratio > curRatio) {
        SMAP_LOGGER_ERROR("migrate ratio %d exceeds pid remote node%d ratio%d.", ratio, nid, curRatio);
        return false;
    }
    return true;
}

static uint64_t GetAttrNidInitMemSize(pid_t pid, int nid)
{
    int nrLocalNuma = GetNrLocalNuma();
    ProcessAttr *attr = GetProcessAttr(pid);
    if (!attr) {
        return -EINVAL;
    }
    int l1node = GetAttrL1(attr);
    return attr->strategyAttr.memSize[l1node][nid - nrLocalNuma];
}

static bool IsRemoteNidMemSizeValid(pid_t pid, int nid, uint64_t memSize)
{
    if (memSize % KB_PER_2MB != 0 || memSize == 0) {
        return false;
    }
    uint64_t curMemSize = GetAttrNidInitMemSize(pid, nid);
    if (curMemSize < 0) {
        return false;
    }
    if (memSize > curMemSize) {
        SMAP_LOGGER_ERROR("migrate memSize %d exceeds pid remote node%d memSize%d.", memSize, nid, curMemSize);
        return false;
    }
    return true;
}

static int SmapMigratePidRemoteNumaCheckInner(struct MigrateEscapeMsg *msg)
{
    pid_t pidArr[MAX_NR_MIGRATE_ESCAPE];
    for (int i = 0; i < msg->count; i++) {
        pidArr[i] = msg->payload[i].pid;
    }

    if (!IsPidArrValid(pidArr, msg->count, false)) {
        SMAP_LOGGER_ERROR("ubturbo_smap_pid_remote_numa_migrate not all pid is valid.");
        return -EINVAL;
    }

    if (!IsScanTypeValid(pidArr, msg->count)) {
        SMAP_LOGGER_ERROR("ubturbo_smap_pid_remote_numa_migrate not all scan type is valid.");
        return -EINVAL;
    }

    int ret = IsPidArrRemoteNumaMatch(msg);
    if (ret) {
        SMAP_LOGGER_ERROR("not all pid remote numa valid, start proc move failed ret: %d.", ret);
        return ret;
    }

    struct ProcessManager *manager = GetProcessManager();
    EnvMutexLock(&manager->lock);
    ret = IsPidArrInState(pidArr, msg->count, PROC_MOVE);
    if (ret != 1) {
        SMAP_LOGGER_ERROR("not all pid in correct state, start proc move failed ret: %d.", ret);
        EnvMutexUnlock(&manager->lock);
        return -EINVAL;
    }
    EnvMutexUnlock(&manager->lock);
    return 0;
}

static int SmapMigratePidRemoteNumaCheck(struct MigrateEscapeMsg *msg)
{
    if (!msg) {
        SMAP_LOGGER_ERROR("msg is null migrate pid remote numa failed.");
        return -EINVAL;
    }

    if (msg->count <= 0 || msg->count > MAX_NR_MIGRATE_ESCAPE) {
        SMAP_LOGGER_ERROR("msg count %d invalid, migrate pid remote numa failed.", msg->count);
        return -EINVAL;
    }

    for (int i = 0; i < msg->count; i++) {
        if (msg->payload[i].srcNid == msg->payload[i].destNid) {
            SMAP_LOGGER_ERROR("srcNid must not equal destNid.");
            return -EINVAL;
        }
        if (!IsRemoteNidValid(msg->payload[i].srcNid)) {
            SMAP_LOGGER_ERROR("[%d] srcNid %d invalid, migrate pid remote numa failed.", i, msg->payload[i].srcNid);
            return -EINVAL;
        }
        if (!IsRemoteNidValid(msg->payload[i].destNid)) {
            SMAP_LOGGER_ERROR("[%d] destNid %d invalid, migrate pid remote numa failed.", i, msg->payload[i].destNid);
            return -EINVAL;
        }

        if (msg->payload[i].migrateMode < MIG_RATIO_MODE || msg->payload[i].migrateMode > MIG_MEMSIZE_MODE) {
            SMAP_LOGGER_ERROR("[%d] pid: %d migrateMode %d invalid.",
                i, msg->payload[i].pid, msg->payload[i].migrateMode);
            return -EINVAL;
        }

        if (GetRunMode() == WATERLINE_MODE && msg->payload[i].migrateMode == MIG_MEMSIZE_MODE) {
            SMAP_LOGGER_ERROR("[%d] smap runMode is WATERLINE_MODE, not supported MIG_MEMSIZE_MODE.", i);
            return -EINVAL;
        }
        if (GetRunMode() == MEM_POOL_MODE && msg->payload[i].migrateMode != MIG_MEMSIZE_MODE) {
            SMAP_LOGGER_ERROR("[%d] smap runMode is MEM_POOL_MODE, not supported mode except MIG_MEMSIZE_MODE.", i);
            return -EINVAL;
        }
        if (msg->payload[i].migrateMode == MIG_RATIO_MODE &&
                !IsRemoteNidRatioValid(msg->payload[i].pid, msg->payload[i].srcNid, msg->payload[i].ratio)) {
            SMAP_LOGGER_ERROR("[%d] pid: %d ratio %d invalid.", i, msg->payload[i].pid, msg->payload[i].ratio);
            return -EINVAL;
        }
        if (msg->payload[i].migrateMode == MIG_MEMSIZE_MODE &&
                !IsRemoteNidMemSizeValid(msg->payload[i].pid, msg->payload[i].srcNid, msg->payload[i].memSize)) {
            SMAP_LOGGER_ERROR("[%d] pid: %d memSize %d invalid.", i, msg->payload[i].pid, msg->payload[i].memSize);
            return -EINVAL;
        }
    }

    return SmapMigratePidRemoteNumaCheckInner(msg);
}

static int BuildMigRemoteNumaMsg(struct MigrateEscapeMsg *msg, struct MigPidRemoteNumaIoctlMsg *ioctlMsg)
{
    int i;

    ioctlMsg->pidCnt = msg->count;
    ioctlMsg->payloads = malloc(msg->count * sizeof(struct MigPayload));
    if (!ioctlMsg->payloads) {
        SMAP_LOGGER_ERROR("malloc pid list failed.");
        return -ENOMEM;
    }
    ioctlMsg->migResArray = calloc(msg->count, sizeof(int));
    if (!ioctlMsg->migResArray) {
        SMAP_LOGGER_ERROR("calloc success pid list failed.");
        free(ioctlMsg->payloads);
        ioctlMsg->payloads = NULL;
        return -ENOMEM;
    }
    for (i = 0; i < msg->count; i++) {
        ioctlMsg->payloads[i].pid = msg->payload[i].pid;
        ioctlMsg->payloads[i].srcNid = msg->payload[i].srcNid;
        ioctlMsg->payloads[i].destNid = msg->payload[i].destNid;
        ioctlMsg->payloads[i].ratio = msg->payload[i].ratio;
        int srcRatio = GetAttrNidInitRatio(msg->payload[i].pid, msg->payload[i].srcNid);
        ioctlMsg->payloads[i].keepRatio = srcRatio - msg->payload[i].ratio;
        ioctlMsg->payloads[i].memSize = msg->payload[i].memSize;
        ioctlMsg->payloads[i].isRatioMode = GetRunMode() == WATERLINE_MODE ? true : false;
    }
    return 0;
}

int ubturbo_smap_pid_remote_numa_migrate(struct MigrateEscapeMsg *msg)
{
    int ret;
    SMAP_LOGGER_INFO("received ubturbo_smap_pid_remote_numa_migrate msg.");
    if (EnvAtomicRead(&g_status) != RUNNING) {
        SMAP_LOGGER_ERROR("smap isn't running, migrate pid remote numa failed.");
        return -EPERM;
    }
    struct MigPidRemoteNumaIoctlMsg ioctlMsg;
    struct ProcessManager *manager = GetProcessManager();

    ret = SmapMigratePidRemoteNumaCheck(msg);
    if (ret) {
        return ret;
    }

    ret = BuildMigRemoteNumaMsg(msg, &ioctlMsg);
    if (ret) {
        SMAP_LOGGER_ERROR("build mig remote numa msg failed. ret: %d", ret);
        return ret;
    }

    ret = ioctl(manager->fds.migrate, SMAP_MIG_PID_REMOTE_NUMA, &ioctlMsg);
    if (ret) {
        SMAP_LOGGER_ERROR("migrate pid remote numa ioctl failed.");
        ret = -REMOTE_MIG_FAIL;
        goto out;
    }

    for (int i = 0; i < ioctlMsg.pidCnt; i++) {
        if (ioctlMsg.migResArray[i] != 1) {
            ret = -REMOTE_MIG_FAIL;
            goto out;
        }
    }

    ret = ChangePidRemoteByPid(&ioctlMsg);
    if (ret) {
        SMAP_LOGGER_ERROR("change pid remote numa failed, ret: %d.", ret);
    }
out:
    free(ioctlMsg.payloads);
    free(ioctlMsg.migResArray);
    SMAP_LOGGER_INFO("ubturbo_smap_pid_remote_numa_migrate done, ret: %d.", ret);
    return ret;
}

static int AssignOldProcessPayload(struct OldProcessPayload *result, ProcessAttr *attr, int l2Node)
{
    int len, l2Index;
    int nrLocalNuma = GetNrLocalNuma();
    int l1Node = GetAttrL1(attr);
    if (l1Node < 0 || l2Node < nrLocalNuma) {
        SMAP_LOGGER_ERROR("AssignOldProcessPayload pid %d L1 %d or L2 %d is invalid.", attr->pid, l1Node, l2Node);
        return -EINVAL;
    }

    l2Index = l2Node - nrLocalNuma;
    len = sizeof(result->l1Node) / sizeof(result->l1Node[0]);
    result->pid = attr->pid;
    result->ratio = attr->strategyAttr.initRemoteMemRatio[l1Node][l2Index];
    result->scanType = attr->scanType;
    result->type = attr->type;
    result->state = attr->state;
    result->l1Node[0] = l1Node;
    result->l2Node[0] = l2Node;
    result->scanTime = attr->scanTime;
    result->migrateMode = attr->migrateMode;
    result->memSize = attr->strategyAttr.memSize[l1Node][l2Index];
    // only the first elem of l1Node and l2Node is used, so assign invalid nid to other elems
    for (int i = 1; i < len; i++) {
        result->l1Node[i] = result->l2Node[i] = NUMA_NO_NODE;
    }
    return 0;
}

static int CheckSmapQueryProcessConfig(int nid, struct OldProcessPayload *result, int inLen, int *outLen)
{
    if (!IsRemoteNidValid(nid)) {
        SMAP_LOGGER_ERROR("ubturbo_smap_process_config_query invalid nid %d.", nid);
        return -EINVAL;
    }
    if (!result) {
        SMAP_LOGGER_ERROR("ubturbo_smap_process_config_query result must not be NULL.");
        return -EINVAL;
    }
    if (inLen <= 0 || inLen > GetCurrentMaxNrPid()) {
        SMAP_LOGGER_ERROR("ubturbo_smap_process_config_query invalid inLen %d.", inLen);
        return -EINVAL;
    }
    if (!outLen) {
        SMAP_LOGGER_ERROR("ubturbo_smap_process_config_query outLen must not be NULL.");
        return -EINVAL;
    }
    return 0;
}

int ubturbo_smap_process_config_query(int nid, struct OldProcessPayload *result, int inLen, int *outLen)
{
    struct ProcessManager *manager = GetProcessManager();
    int i = 0;
    int ret = 0;

    SMAP_LOGGER_INFO("received ubturbo_smap_process_config_query msg, nid:%d, inLen:%d.", nid, inLen);
    if (EnvAtomicRead(&g_status) != RUNNING) {
        SMAP_LOGGER_ERROR("ubturbo_smap_process_config_query smap uninitialized.");
        return -EPERM;
    }
    SMAP_LOGGER_INFO("ubturbo_smap_process_config_query nid: %d, inLen %d.", nid, inLen);
    ret = CheckSmapQueryProcessConfig(nid, result, inLen, outLen);
    if (ret) {
        SMAP_LOGGER_INFO("Check ubturbo_smap_process_config_query msg error %d.", ret);
        return ret;
    }
    SMAP_LOGGER_INFO("Check ubturbo_smap_process_config_query msg done.");

    EnvMutexLock(&manager->lock);
    for (ProcessAttr *attr = manager->processes; attr; attr = attr->next) {
        if (!InAttrL2(attr, nid)) {
            SMAP_LOGGER_INFO("Skip pid %d because L2 node != %d.", attr->pid, nid);
            continue;
        }
        ret = AssignOldProcessPayload(&result[i], attr, nid);
        if (ret) {
            SMAP_LOGGER_ERROR("Smap pid %d config is invalid.", attr->pid);
            break;
        }
        SMAP_LOGGER_INFO("Pid %d data packed.", attr->pid);
        if (++i >= inLen) {
            SMAP_LOGGER_INFO("Reach result length %d.", inLen);
            break;
        }
    }
    *outLen = i;
    SMAP_LOGGER_INFO("ubturbo_smap_process_config_query done, outLen %d.", *outLen);
    EnvMutexUnlock(&manager->lock);
    return ret;
}

int ubturbo_smap_remote_numa_freq_query(uint16_t *numa, uint64_t *freq, uint16_t length)
{
    int ret;
    uint16_t i;
    struct ProcessManager *manager = GetProcessManager();

    SMAP_LOGGER_INFO("Received ubturbo_smap_remote_numa_freq_query msg, length:%u.", length);
    if (EnvAtomicRead(&g_status) != RUNNING) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_freq_query smap uninitialized.");
        return -EPERM;
    }
    if (!numa || !freq) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_freq_query numa or freq is NULL.");
        return -EINVAL;
    }
    if (length == 0 || length > REMOTE_NUMA_BITS) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_freq_query length %u invalid.", length);
        return -EINVAL;
    }
    for (i = 0; i < length; i++) {
        if (!IsRemoteNidValid(numa[i])) {
            SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_freq_query nid %u invalid.", numa[i]);
            return -EINVAL;
        }
    }
    ret = memset_s(freq, length * sizeof(uint64_t), 0, length * sizeof(uint64_t));
    if (ret) {
        SMAP_LOGGER_ERROR("ubturbo_smap_remote_numa_freq_query memset_s failed, ret: %d.", ret);
        return -ENOMEM;
    }
    EnvMutexLock(&manager->lock);
    ProcessAttr *current;
    for (i = 0; i < length; i++) {
        for (current = manager->processes; current; current = current->next) {
            if (InAttrL2(current, numa[i])) {
                freq[i] += current->scanAttr.actCount[numa[i]].freqSum;
            }
        }
    }
    EnvMutexUnlock(&manager->lock);
    SMAP_LOGGER_INFO("ubturbo_smap_remote_numa_freq_query success.");
    return 0;
}
