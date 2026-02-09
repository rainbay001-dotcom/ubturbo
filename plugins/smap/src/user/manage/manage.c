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
#define _GNU_SOURCE
#include <fcntl.h>
#include <inttypes.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>

#include "smap_user_log.h"
#include "securec.h"
#include "device.h"
#include "access_ioctl.h"
#include "virt.h"
#include "advanced-strategy/scene.h"
#include "smap_config.h"
#include "strategy/period_config.h"
#include "strategy/strategy.h"
#include "strategy/migration.h"
#include "manage.h"

static struct ProcessManager g_processManager;

static char g_mmapTypeName[][MMAP_TYPE_STRING_LEN] = { "mmap_private", "mmap_shared" };
static char *g_nodePattern[LOCAL_NUMA_NUM] = { " N0=", " N1=", " N2=", " N3=" };

EnvAtomic g_forbiddenNodes[MAX_NODES];
RunMode g_runMode;

RunMode GetRunMode(void)
{
    return g_runMode;
}

void SetRunMode(RunMode runMode)
{
    g_runMode = runMode;
}

PidType GetPidType(struct ProcessManager *manager)
{
    return manager->tracking.pageSize == PAGESIZE_4K ? PROCESS_TYPE : VM_TYPE;
}

static int RemoteNumaInfoInit(void)
{
    EnvMutexInit(&g_processManager.remoteNumaInfo.lock);
    for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
        g_processManager.remoteNumaInfo.sharedSize[j] = 0;
        g_processManager.remoteNumaInfo.usedInfo[j].ifUsedFreshed = false;
        g_processManager.remoteNumaInfo.usedInfo[j].used = 0;
        g_processManager.remoteNumaInfo.usedInfo[j].size = 0;
        for (int i = 0; i < LOCAL_NUMA_NUM; i++) {
            g_processManager.remoteNumaInfo.privateSize[i][j] = 0;
            g_processManager.remoteNumaInfo.privateUsedInfo[i][j].ifUsedFreshed = false;
            g_processManager.remoteNumaInfo.privateUsedInfo[i][j].used = 0;
            g_processManager.remoteNumaInfo.privateUsedInfo[i][j].size = 0;
        }
    }
}

int GetNrLocalNuma(void)
{
    return g_processManager.nrLocalNuma;
}

int ProcessManagerInit(uint32_t pageType)
{
    int i;
    int ret = memset_s(&g_processManager, sizeof(struct ProcessManager), 0, sizeof(struct ProcessManager));
    if (ret != EOK) {
        SMAP_LOGGER_ERROR("Clear process manager memory failed: %d.", ret);
        return -ret;
    }
    ret = GeneratePeriodConfigFile(PERIOD_CONFIG_PATH);
    if (ret != 0) {
        SMAP_LOGGER_ERROR("Generat period config file failed, ret is %d.", ret);
    }
    g_processManager.tracking.pageSize = (pageType == PAGETYPE_4K) ? PAGESIZE_4K : PAGESIZE_2M;

    for (i = 0; i < MAX_NODES; i++) {
        g_processManager.fds.nodes[i] = DEFAULT_FD;
    }
    g_processManager.fds.migrate = DEFAULT_FD;
    g_processManager.fds.access = DEFAULT_FD;
    g_processManager.fds.lock = DEFAULT_FD;
    for (i = 0; i < MAX_THREADS; i++) {
        g_processManager.threadCtx[i] = NULL;
    }
    g_processManager.processes = NULL;
    RemoteNumaInfoInit();
    EnvMutexInit(&g_processManager.lock);
    EnvMutexInit(&g_processManager.threadLock);
    InitSceneInfo(&g_processManager.sceneInfo);
    g_runMode = WATERLINE_MODE;
    return 0;
}

int LoadMangerNrProcessNum(void)
{
    return g_processManager.nr[PROCESS_TYPE];
}

int LoadMangerNrVmNum(void)
{
    return g_processManager.nr[VM_TYPE];
}

bool PidIsValid(pid_t pid)
{
    char path[32];
    int ret = snprintf_s(path, sizeof(path), sizeof(path), "/proc/%d", pid);
    if (ret == -1) {
        return false;
    }
    return access(path, F_OK) == 0;
}

int IsQemuTask(pid_t pid)
{
    char comm[BUFFER_SIZE];
    char cmdBuf[BUFFER_SIZE];
    int ret = snprintf_s(cmdBuf, sizeof(cmdBuf), sizeof(cmdBuf) - 1, "%s %d comm %s", CAT_SCRIPT_CAT_PATH, pid,
                         CAT_SCRIPT_TAIL);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("Failed to generate cmd string, ret is %d.", ret);
        return -EINVAL;
    }
    SMAP_LOGGER_INFO("Before open comm file");
    FILE *file = popen(cmdBuf, "r");
    if (!file) {
        SMAP_LOGGER_ERROR("Failed to open file, errno is %d.", errno);
        return -EINVAL;
    }
    if (fgets(comm, sizeof(comm), file)) {
        SMAP_LOGGER_DEBUG("Skip the first line of comm file.");
    }
    if (fgets(comm, sizeof(comm), file)) {
        SMAP_LOGGER_INFO("After fgets comm file");
        pclose(file);
        if ((strncmp(comm, VM_NAME_STR, PID_NAME_LEN) == 0) ||
            (strncmp(comm, VM_KVM_NAME_STR, PID_KVM_NAME_LEN) == 0)) {
            ret = VM_TYPE;
        } else {
            ret = PROCESS_TYPE;
        }
        return ret;
    }
    SMAP_LOGGER_ERROR("Error occur in fgets comm file");

    (void)pclose(file);
    return -1;
}

void LinkedListAdd(ProcessAttr **head, ProcessAttr **add)
{
    (*add)->next = *head;
    *head = *add;
}

static void LinkedListAddSafe(ProcessAttr **head, ProcessAttr **add, EnvMutex *lock)
{
    EnvMutexLock(lock);
    (*add)->next = *head;
    *head = *add;
    EnvMutexUnlock(lock);
}

static void ResetActcData(ActcData *actcData[], int len)
{
    for (int i = 0; i < len; i++) {
        if (actcData[i]) {
            free(actcData[i]);
            actcData[i] = NULL;
        }
    }
}

static void FreeProceccesAttr(ProcessAttr *attr)
{
    if (attr == NULL) {
        return;
    }
    if (attr->scanAttr.actcData) {
        ResetActcData(attr->scanAttr.actcData, MAX_NODES);
    }
    free(attr);
}

void LinkedListRemove(ProcessAttr **remove, ProcessAttr **head)
{
    if (*head == NULL || *remove == NULL) {
        return;
    }

    ProcessAttr *toRemove = *remove;

    if (*head == toRemove) {
        *head = toRemove->next;
        toRemove->next = NULL;
        FreeProceccesAttr(toRemove);
        return;
    }

    ProcessAttr *prev = *head;
    while (prev->next != NULL && prev->next != toRemove) {
        prev = prev->next;
    }
    if (prev->next == toRemove) {
        prev->next = toRemove->next;
        toRemove->next = NULL;
        FreeProceccesAttr(toRemove);
        *remove = NULL;
    }
}

static void ResetActcDataForPid(ProcessAttr *attr)
{
    ResetActcData(attr->scanAttr.actcData, MAX_NODES);
}

static int InitPidActcData(ProcessAttr *attr)
{
    if (attr->walkPage.nrPage == 0) {
        SMAP_LOGGER_ERROR("Get pid %d nr pages failed.", attr->pid);
        return -EINVAL;
    }
    ActcData *actc[MAX_NODES] = { 0 };
    for (int i = 0; i < MAX_NODES; i++) {
        if (attr->walkPage.nrPages[i] == 0) {
            continue;
        }
        actc[i] = calloc(attr->walkPage.nrPages[i], sizeof(ActcData));
        if (!actc[i]) {
            ResetActcData(actc, MAX_NODES);
            return -ENOMEM;
        }
    }

    ResetActcDataForPid(attr);
    for (int i = 0; i < MAX_NODES; i++) {
        attr->scanAttr.actcData[i] = actc[i];
    }
    attr->isLowMem = false;
    return 0;
}

static unsigned long ProcessSmapsFile(pid_t pid, const char *targetLinePrefix, size_t prefixLength, size_t divisor)
{
    char filename[BUFFER_SIZE];
    int ret = snprintf_s(filename, sizeof(filename), sizeof(filename), "/proc/%d/smaps", pid);
    if (ret == -1) {
        return 0;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        SMAP_LOGGER_ERROR("fopen /proc/%d/smaps failed.", pid);
        return 0;
    }

    char line[BUFFER_SIZE];
    unsigned long totalPages = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, targetLinePrefix, prefixLength) == 0) {
            unsigned long value;
            if (sscanf_s(line + prefixLength, "%lu", &value) != 1) {
                continue;
            }
            totalPages += value * KB / divisor; // KB to pages
        }
    }

    ret = fclose(file);
    if (ret) {
        SMAP_LOGGER_ERROR("close smaps failed: %d.", ret);
    }
    return totalPages;
}

static unsigned long GetNormalPageCount(pid_t pid)
{
    return ProcessSmapsFile(pid, RSS_LINE_PREFIX, RSS_LINE_PREFIX_LENGTH, PAGE_SIZE);
}

static unsigned long GetHugePageCount(pid_t pid)
{
    return ProcessSmapsFile(pid, HUGETLB_LINE_PREFIX, HUGETLB_LINE_PREFIX_LENGTH, TWO_MEGA_SIZE);
}

unsigned long GetPidNrPages(pid_t pid)
{
    return (g_processManager.tracking.pageSize == PAGESIZE_2M) ? GetHugePageCount(pid) : GetNormalPageCount(pid);
}

static int GetNodeFromCpu(int cpu)
{
    int ret;
    char path[BUFFER_SIZE];
    for (int node = 0; node < MAX_NODES; node++) {
        ret = snprintf_s(path, sizeof(path), sizeof(path), CPU_NUMA_PATH, cpu, node);
        if (ret == -1) {
            return -EINVAL;
        }
        if (access(path, F_OK) == 0) {
            return node;
        }
    }
    SMAP_LOGGER_ERROR("open cpu %d node failed.", cpu);
    return -EINVAL;
}

int GetNumaNodesForPid(pid_t pid, int *node)
{
    int ret;
    int cpuNode;
    cpu_set_t mask;
    int i;

    CPU_ZERO(&mask);
    ret = sched_getaffinity(pid, sizeof(cpu_set_t), &mask);
    if (ret) {
        SMAP_LOGGER_ERROR("pid %d sched_getaffinity failed: %d.", pid, ret);
        return -EINVAL;
    }
    for (i = 0; i < sizeof(cpu_set_t) * BIT_TO_BYTE; i++) {
        if (CPU_ISSET(i, &mask)) {
            cpuNode = GetNodeFromCpu(i);
            if (cpuNode == -EINVAL) {
                SMAP_LOGGER_ERROR("pid % get node from cpu failed: %d.", pid, ret);
                return -EINVAL;
            }
            *node = cpuNode;
            break;
        }
    }
    return 0;
}

bool IsHugeMode(void)
{
    return g_processManager.tracking.pageSize == PAGESIZE_2M;
}

bool IsHugeAligned(uint64_t addr)
{
    return (addr & (TWO_MEGA_SIZE - 1)) == 0;
}

int IsHugePageRange(const char *line)
{
    return strstr(line, "hugepage") != NULL;
}

static inline void ClearActcInfo(ProcessAttr *attr, int nid)
{
    attr->scanAttr.actcLen[nid] = 0;
    (void)memset_s(&attr->scanAttr.actCount[nid], sizeof(ActCount), 0, sizeof(ActCount));
}

static int FillActcByBitmap(ProcessAttr *attr, int nid, struct ProcessMemBitmap *pmb, struct AccessPidFreq *apf)
{
    size_t i, nrFreq, nrBit, acidx = 0;
    uint16_t freqMax = 0, freqMin = UINT16_MAX;
    uint64_t actcLen, paddr, freqSum = 0;
    size_t len = pmb->len[nid];
    unsigned long *bitmap = pmb->data[nid];
    unsigned long *whiteListBitmap = pmb->whiteListBm[nid];
    if (attr->walkPage.nrPages[nid] == 0) {
        ClearActcInfo(attr, nid);
        return 0;
    }
    ActcData *actc = attr->scanAttr.actcData[nid];

    nrFreq = nrBit = actcLen = 0;
    for (acidx = 0; acidx < BITS_PER_LONG * len; acidx++) {
        if (actcLen >= attr->walkPage.nrPages[nid] || actcLen >= apf->len[nid]) {
            break;
        }
        if (!TestBit(acidx, bitmap)) {
            continue;
        }
        nrBit++;
        if (pmb->vmSize && pmb->mapping) {
            actc[actcLen].prior = pmb->mapping[pmb->mappingOffset + actcLen] & 0xff;
        } else {
            actc[actcLen].prior = 0;
        }
        if (TestBit(acidx, whiteListBitmap)) {
            actc[actcLen].isWhiteListPage = true;
        }
        actc[actcLen].freq = apf->freq[nid][actcLen];
        if (actc[actcLen].freq != 0) {
            nrFreq++;
            freqSum += actc[actcLen].freq;
        }
        actc[actcLen].addr = actcLen;
        freqMax = MAX(freqMax, actc[actcLen].freq);
        freqMin = MIN(freqMin, actc[actcLen].freq);
        actcLen++;
    }
    attr->scanAttr.actcLen[nid] = actcLen;
    attr->scanAttr.actCount[nid].freqMax = freqMax;
    attr->scanAttr.actCount[nid].freqMin = freqMin;
    attr->scanAttr.actCount[nid].freqNum = nrFreq;
    attr->scanAttr.actCount[nid].freqSum = freqSum;
    attr->scanAttr.actCount[nid].pageNum = attr->scanAttr.actcLen[nid];
    attr->scanAttr.actCount[nid].freqZero = attr->scanAttr.actcLen[nid] - nrFreq;
    SMAP_LOGGER_INFO("Node%d actcLen %llu, nrFreq %zu, nrBit %zu, freqMax %d, freqMin %d, freqSum %lu.", nid, actcLen,
                     nrFreq, nrBit, freqMax, freqMin, freqSum);
    return 0;
}

static int MappingAscFunc(const void *map1, const void *map2)
{
    uint32_t m1 = *(uint32_t *)map1;
    uint32_t m2 = *(uint32_t *)map2;

    if (m1 == m2) {
        return 0;
    }
    return m1 < m2 ? -1 : 1;
}

static int FillActcData(ProcessAttr *attr, struct ProcessMemBitmap *pmb, struct AccessPidFreq *apf)
{
    int ret;
    if (!pmb) {
        SMAP_LOGGER_ERROR("FillActcData pmb is null.");
        return -EINVAL;
    }
    if (pmb->mapping) {
        qsort(pmb->mapping, pmb->vmSize, sizeof(*pmb->mapping), MappingAscFunc);
    }
    for (int nid = 0; nid < MAX_NODES; nid++) {
        attr->scanAttr.actcLen[nid] = 0;
        ret = FillActcByBitmap(attr, nid, pmb, apf);
        if (ret) {
            return ret;
        }
        pmb->mappingOffset += attr->scanAttr.actcLen[nid];
    }
    return 0;
}

static int CheckPid(pid_t pid)
{
    PidType type = GetPidType(&g_processManager);
    int ret;
    if (!PidIsValid(pid)) {
        SMAP_LOGGER_ERROR("Input pid %d is invalid.", pid);
        return -ESRCH;
    }
    ret = IsQemuTask(pid);
    if (ret != type) {
        SMAP_LOGGER_ERROR("Pid %d type(%d) conflict with current pid type(%d).", pid, ret, type);
        return -EINVAL;
    }
    return 0;
}

ProcessAttr *GetProcessAttr(pid_t pid)
{
    ProcessAttr *current = g_processManager.processes;
    EnvMutexLock(&g_processManager.lock);
    while (current && current->pid != pid) {
        current = current->next;
    }
    EnvMutexUnlock(&g_processManager.lock);
    return current;
}

/* 调用前必须持有锁g_processManager.lock */
ProcessAttr *GetProcessAttrLocked(pid_t pid)
{
    ProcessAttr *current = g_processManager.processes;
    while (current && current->pid != pid) {
        current = current->next;
    }
    return current;
}

static int ParseMmapType(int domainId, MmapType *mmapType)
{
    int err;
    char *xml;
    int retry = 10;

    do {
        xml = GetXMLByDomainId(domainId);
    } while (retry-- && !xml);
    if (!xml) {
        *mmapType = MMAP_SHARED;
        return -EINVAL;
    }
    if (strstr(xml, MMAP_TYPE_SHARED_SEG1) || strstr(xml, MMAP_TYPE_SHARED_SEG2)) {
        *mmapType = MMAP_SHARED;
    }
    free(xml);
    return 0;
}

int VMPreprocess(pid_t pid, ProcessAttr *attr)
{
    int ret;

    if (GetPidType(&g_processManager) == VM_TYPE) {
        ret = ReadDomainIdByPid(pid, &attr->vmPidAttr.domainId);
        if (ret) {
            SMAP_LOGGER_ERROR("Read domain id of pid %d error: %d.", pid, ret);
            return -EINVAL;
        }
        SMAP_LOGGER_INFO("Read domain id of pid %d: %d.", pid, attr->vmPidAttr.domainId);
        ret = ParseMmapType(attr->vmPidAttr.domainId, &attr->vmPidAttr.mmapType);
        if (ret) {
            SMAP_LOGGER_ERROR("Parse mmap type of pid %d failed.", pid);
            return 0;
        }
        SMAP_LOGGER_INFO("Read Mmap type of pid %d: %s.", pid, g_mmapTypeName[attr->vmPidAttr.mmapType]);
    }
    return 0;
}

static void SetProcessConfig(ProcessAttr *attr, ProcessParam *param)
{
    attr->pid = param->pid;
    attr->scanTime = param->scanTime;
    attr->duration = param->duration;
    attr->scanType = param->scanType;
    attr->migrateMode = param->numaParam[0].migrateMode;
    attr->remoteNumaCnt = param->count;
    attr->enableSwap = true;
    attr->initLocalMemRatio = HUNDRED;
    for (int i = 0; i < param->count; i++) {
        attr->initLocalMemRatio -= param->numaParam[i].ratio;
    }
    if (time(&attr->scanStart) == (time_t)-1) {
        SMAP_LOGGER_ERROR("get time error");
    }
    int nrLocalNuma = GetNrLocalNuma();
    SMAP_LOGGER_INFO("attr->scanStart time: %s", ctime(&attr->scanStart));
    int localNumaCnt = GetL1Count(attr->numaAttr.numaNodes);
    SMAP_LOGGER_INFO("Pid: %d local numa cnt: %d.", attr->pid, localNumaCnt);
    SMAP_LOGGER_INFO("Pid: %d remote numa cnt: %d.", attr->pid, attr->remoteNumaCnt);
    if ((param->count > 1 || localNumaCnt > 1) && GetPidType(&g_processManager) == VM_TYPE) { // multinuma vm
        for (int i = 0; i < param->count; i++) {
            attr->migrateParam[i].nid = param->numaParam[i].nid;
            attr->migrateParam[i].memSize = param->numaParam[i].memSize;
            SMAP_LOGGER_INFO("Multinuma vm destNid: %d, memSize: %lu", attr->migrateParam[i].nid,
                             attr->migrateParam[i].memSize);
            
            for (int j = 0; j < nrLocalNuma && j < LOCAL_NUMA_NUM; j++) {
                attr->strategyAttr.initRemoteMemRatio[j][param->numaParam[i].nid - nrLocalNuma] =
                    param->numaParam[i].ratio;
                SMAP_LOGGER_INFO("Multinuma vm destNid: %d, ratio: %lu", param->numaParam[i].nid,
                                 param->numaParam[i].ratio);
            }
            AddAttrL2(attr, param->numaParam[i].nid);
        }
    } else {
        if (param->numaParam[0].nid < nrLocalNuma || param->numaParam[0].nid >= nrLocalNuma + REMOTE_NUMA_NUM) {
            return;
        }
        for (int i = 0; i < nrLocalNuma && i < LOCAL_NUMA_NUM; i++) {
            attr->strategyAttr.initRemoteMemRatio[i][param->numaParam[0].nid - nrLocalNuma] =
                param->numaParam[0].ratio;
            if (EqualToAttrL1(attr, i)) {
                attr->migrateParam[0].memSize = param->numaParam[0].memSize;
                attr->migrateParam[0].nid = param->numaParam[0].nid;
                attr->strategyAttr.memSize[i][param->numaParam[0].nid - nrLocalNuma] = param->numaParam[0].memSize;
            }
        }
        SetAttrL2(attr, param->numaParam[0].nid);
    }
}

int AddProcess(ProcessParam *param, PidType type, uint32_t *nodeBitmap)
{
    int ret;
    if (g_processManager.nr[type] >= GetCurrentMaxNrPid()) {
        SMAP_LOGGER_ERROR("nr of pid is out of limit.");
        return -EINVAL;
    }

    ProcessAttr *attr = calloc(1, sizeof(ProcessAttr));
    if (!attr) {
        SMAP_LOGGER_ERROR("Alloc memory for process failed.");
        return -ENOMEM;
    }

    if (!nodeBitmap) {
        ret = SetProcessLocalNuma(param->pid, &attr->numaAttr.numaNodes, type == VM_TYPE);
        if (ret) {
            SMAP_LOGGER_ERROR("Query pid %d memory usage failed: %d.", param->pid, ret);
            free(attr);
            return ret;
        }
    } else {
        attr->numaAttr.numaNodes = *nodeBitmap;
    }

    if (param->scanType == NORMAL_SCAN) {
        ret = VMPreprocess(param->pid, attr);
        if (ret) {
            SMAP_LOGGER_ERROR("Preprocess VM process %d attribute failed, return code: %d.", param->pid, ret);
            free(attr);
            return ret;
        }
    } else if (param->scanType == HAM_SCAN || param->scanType == STATISTIC_SCAN) {
        attr->state = PROC_MOVE;
        SMAP_LOGGER_INFO("Set pid %d state to %d.", param->pid, PROC_MOVE);
    }

    attr->type = type;
    SetProcessConfig(attr, param);
    LinkedListAdd(&g_processManager.processes, &attr);
    SMAP_LOGGER_INFO("Set pid %d scan cycle to %ums.", attr->pid, attr->scanTime);
    g_processManager.nr[type]++;

    ret = SyncAllProcessConfig();
    if (ret) {
        SMAP_LOGGER_WARNING("Synchronize pid %d config maybe failed: %d.", param->pid, ret);
    }
    SMAP_LOGGER_INFO("Add pid:%d success! localMemRatio:%d, migrateMode: %d.", param->pid, attr->initLocalMemRatio,
                     attr->migrateMode);

    return 0;
}

int SetLocalNumaByCpu(pid_t pid, uint32_t *nodeBitmap)
{
    int ret;
    int nid;
    cpu_set_t mask;

    if (!nodeBitmap) {
        SMAP_LOGGER_ERROR("Get pid %d nodeBitmap is null", pid);
        return -EINVAL;
    }

    CPU_ZERO(&mask);
    ret = sched_getaffinity(pid, sizeof(cpu_set_t), &mask);
    if (ret) {
        SMAP_LOGGER_ERROR("Get pid %d sched affinity failed: %d.", pid, ret);
        return -EINVAL;
    }
    for (int i = 0; i < sizeof(cpu_set_t) * BIT_TO_BYTE; i++) {
        if (!CPU_ISSET(i, &mask)) {
            continue;
        }
        nid = GetNodeFromCpu(i);
        if (nid == -EINVAL) {
            SMAP_LOGGER_ERROR("Get node from cpu%d failed: %d.", i, ret);
            return -EINVAL;
        }
        AddL1(nodeBitmap, nid);
    }
    return 0;
}

FILE *OpenNumaMaps(pid_t pid)
{
    char cmdBuf[BUFFER_SIZE];
    int ret = snprintf_s(cmdBuf, sizeof(cmdBuf), sizeof(cmdBuf) - 1, "%s %d numa_maps %s", CAT_SCRIPT_CAT_PATH, pid,
                         CAT_SCRIPT_TAIL);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("OpenNumaMaps for pid %d err.", pid);
        return NULL;
    }
    FILE *fp = popen(cmdBuf, "r");
    if (!fp) {
        SMAP_LOGGER_ERROR("OpenNumaMaps fopen failed: %d.", -errno);
    }
    return fp;
}

static void SetLocalByNumaMaps(char *line, uint32_t *nodeBitmap, bool hugeFlag)
{
    int i;
    int nrLocalNuma = GetNrLocalNuma();
    char *substr = NULL;

    /*
     * It's possible that there are multiple Nx= in one line,
     * so it's necessary to traverse all node
     */
    for (i = 0; i < nrLocalNuma; i++) {
        if (hugeFlag && !IsNumaMapLineHuge(line)) {
            continue;
        }
        substr = strstr(line, g_nodePattern[i]);
        if (substr) {
            AddL1(nodeBitmap, i);
        }
    }
}

static int SetLocalNumaByNumaMaps(pid_t pid, uint32_t *nodeBitmap, bool hugeFlag)
{
    FILE *fp;
    char line[MAX_LINE_LENGTH];

    SMAP_LOGGER_INFO("Before open numa_maps");
    fp = OpenNumaMaps(pid);
    if (!fp) {
        SMAP_LOGGER_ERROR("SetLocalNumaByNumaMaps open numa maps failed.");
        return -EINVAL;
    }

    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        SetLocalByNumaMaps(line, nodeBitmap, hugeFlag);
    }
    SMAP_LOGGER_INFO("After fgets numa_maps");

    if (pclose(fp)) {
        SMAP_LOGGER_ERROR("SetLocalNumaByNumaMaps close numa maps failed.");
    }
    return 0;
}

int SetProcessLocalNuma(pid_t pid, uint32_t *nodeBitmap, bool hugeFlag)
{
    int ret1, ret2;

    ret1 = SetLocalNumaByCpu(pid, nodeBitmap);
    if (ret1) {
        SMAP_LOGGER_WARNING("Set pid %d local numa by cpu failed: %d.", pid, ret1);
    }
    SMAP_LOGGER_INFO("pid %d node bitmap after set local numa by cpu: %#x.", pid, *nodeBitmap);
    ret2 = SetLocalNumaByNumaMaps(pid, nodeBitmap, hugeFlag);
    if (ret2) {
        SMAP_LOGGER_WARNING("Set pid %d local numa by numa maps failed: %d.", pid, ret2);
    }
    SMAP_LOGGER_INFO("pid %d node bitmap after set local numa by numa maps: %#x.", pid, *nodeBitmap);

    return ret1 & ret2;
}

static void PrintProcessNuma(ProcessAttr *attr)
{
    int i;
    int ret;
    char output[MAX_LINE_LENGTH] = { 0 };
    int len = sizeof(output) / sizeof(char);
    char *result = output;
    char *tmpl = "Node00 ";

    for (i = 0; i < MAX_NODES; i++) {
        if (InAttrL1(attr, i) || InAttrL2(attr, i)) {
            ret = snprintf_s(result, len, strlen(tmpl), "Node%2d ", i);
            if (ret > 0) {
                len -= strlen(tmpl);
                result += strlen(tmpl);
            }
        }
    }
    SMAP_LOGGER_INFO("pid %d is using %s.", attr->pid, output);
}

int ProcessAddManage(ProcessParam *param, uint32_t *nodeBitmap)
{
    ProcessAttr *current = g_processManager.processes;
    PidType type = GetPidType(&g_processManager);
    int ret = CheckPid(param->pid);
    if (ret) {
        SMAP_LOGGER_ERROR("pid %d check failed: %d.", param->pid, ret);
        return ret;
    }
    current = GetProcessAttrLocked(param->pid);
    if (current) {
        SetProcessConfig(current, param);
        SMAP_LOGGER_INFO("Set pid %d scan cycle to %ums.", current->pid, current->scanTime);
        ret = SyncAllProcessConfig();
        if (ret) {
            SMAP_LOGGER_WARNING("Synchronize pid %d config maybe failed: %d.", param->pid, ret);
        }
        for (int i = 0; i < param->count; i++) {
            SMAP_LOGGER_INFO("Update pid:%d success! migrateMode: %d, destnid: %d, memSize: %llu.",
                             current->pid, current->migrateMode,
                             current->migrateParam[i].nid, current->migrateParam[i].memSize);
        }
    } else {
        ret = AddProcess(param, type, nodeBitmap);
        if (ret) {
            SMAP_LOGGER_ERROR("Add pid %d to list failed: %d.", param->pid, ret);
            return ret;
        }
        SMAP_LOGGER_INFO("Add pid %d to list done.", param->pid);
    }

    return 0;
}

static void ClearRemoteMemUsed(void)
{
    for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
        g_processManager.remoteNumaInfo.usedInfo[j].used = 0;
        g_processManager.remoteNumaInfo.usedInfo[j].ifUsedFreshed = true;
        for (int i = 0; i < LOCAL_NUMA_NUM; i++) {
            g_processManager.remoteNumaInfo.privateUsedInfo[i][j].used = 0;
            g_processManager.remoteNumaInfo.privateUsedInfo[i][j].ifUsedFreshed = true;
        }
    }
    SMAP_LOGGER_DEBUG("Smap clear remote mem used end.");
}

static void CalRemoteMemUsed(void)
{
    ProcessAttr *attr = g_processManager.processes;
    struct RemoteNumaInfo *remoteNumaInfo = &g_processManager.remoteNumaInfo;
    int i, j;

    int nrLocal = g_processManager.nrLocalNuma;
    // 计算每个本地远端对应按照ratio可迁出的最大量
    while (attr) {
        for (j = 0; j < REMOTE_NUMA_NUM; j++) {
            remoteNumaInfo->usedInfo[j].used += attr->walkPage.nrPages[j + nrLocal];
            SMAP_LOGGER_DEBUG("usedInfo[%d].used: %llu, nrPages[%d+%d]: %u.", j, remoteNumaInfo->usedInfo[j].used, j,
                              nrLocal, attr->walkPage.nrPages[j + nrLocal]);
            for (i = 0; i < LOCAL_NUMA_NUM; i++) {
                remoteNumaInfo->privateUsedInfo[i][j].used += attr->strategyAttr.allocRemoteNrPages[i][j];
                SMAP_LOGGER_DEBUG("privateUsedInfo[%d][%d].used: %llu, allocRemoteNrPages[%d][%d]: %u.", i, j,
                                  remoteNumaInfo->privateUsedInfo[i][j].used, i, j,
                                  attr->strategyAttr.allocRemoteNrPages[i][j]);
            }
        }
        attr = attr->next;
    }
}

void CheckAndRemoveInvalidProcess(void)
{
    struct RemoteNumaInfo *numaInfo;
    PidType type = GetPidType(&g_processManager);

    EnvMutexLock(&g_processManager.lock);
    for (ProcessAttr *attr = g_processManager.processes; attr;) {
        pid_t pid = attr->pid;
        ProcessAttr *next = attr->next;
        SMAP_LOGGER_INFO("check if pid %d is valid.", pid);
        if (!PidIsValid(pid)) {
            // send ioctl to remove pid
            struct AccessRemovePidPayload payload = { .pid = pid };
            int ret = AccessIoctlRemovePid(1, &payload);
            if (ret) {
                SMAP_LOGGER_ERROR("access ioctl remove pid %d error: %d.", pid, ret);
            }

            LinkedListRemove(&attr, &g_processManager.processes);
            g_processManager.nr[type]--;
            ret = SyncAllProcessConfig();
            if (ret) {
                SMAP_LOGGER_WARNING("Synchronize pid %d config maybe failed: %d.", pid, ret);
            }
            SMAP_LOGGER_INFO("remove pid %d from managed process.", pid);
        }
        attr = next;
    }
    if (!g_processManager.processes) {
        numaInfo = &g_processManager.remoteNumaInfo;
        EnvMutexLock(&numaInfo->lock);
        ClearRemoteMemUsed();
        SMAP_LOGGER_DEBUG("Remote memory usage cleared.");
        EnvMutexUnlock(&numaInfo->lock);
    }
    EnvMutexUnlock(&g_processManager.lock);
}

void RemoveManagedProcess(int nr, pid_t *pidArr)
{
    int ret;
    PidType type = GetPidType(&g_processManager);
    for (int i = 0; i < nr; i++) {
        ProcessAttr *attr = g_processManager.processes;
        while (attr && attr->pid != pidArr[i]) {
            attr = attr->next;
        }
        if (!attr) {
            SMAP_LOGGER_WARNING("pid: %d, not exist, not need to remove.", pidArr[i]);
            continue;
        }
        LinkedListRemove(&attr, &g_processManager.processes);
        SMAP_LOGGER_INFO("Remove pid: %d, from managed process.", pidArr[i]);
        g_processManager.nr[type]--;
        ret = SyncAllProcessConfig();
        if (ret) {
            SMAP_LOGGER_WARNING("Synchronize pid %d config maybe failed: %d.", pidArr[i], ret);
        }
    }
}

void RemoveAllManagedProcess(void)
{
    int ret = AccessIoctlRemoveAllPid();
    if (ret) {
        SMAP_LOGGER_ERROR("access ioctl remove all pid error: %d.", ret);
    }
    EnvMutexLock(&g_processManager.lock);
    ProcessAttr *attr = g_processManager.processes;
    while (attr) {
        SMAP_LOGGER_INFO("During destruction remove pid: %d, from managed process.", attr->pid);
        LinkedListRemove(&attr, &g_processManager.processes);
        attr = g_processManager.processes;
    }
    EnvMutexUnlock(&g_processManager.lock);
    g_processManager.processes = NULL;
    g_processManager.nr[VM_TYPE] = g_processManager.nr[PROCESS_TYPE] = 0;
}

pid_t *QueryManagedProcess(PidType type)
{
    EnvMutexLock(&g_processManager.lock);
    int nrProcess = g_processManager.nr[type];
    if (!nrProcess) {
        SMAP_LOGGER_INFO("No %s-type process is managed.", type == VM_TYPE ? "vm" : "process");
        EnvMutexUnlock(&g_processManager.lock);
        return NULL;
    }

    ProcessAttr *current = g_processManager.processes;
    pid_t *managedPid = malloc(sizeof(pid_t) * nrProcess);
    if (!managedPid) {
        SMAP_LOGGER_ERROR("Alloc mem for query-managed-process failed, stopped.");
        EnvMutexUnlock(&g_processManager.lock);
        return NULL;
    }
    int nrManaged = 0;
    while (current) {
        if (current->type = type) {
            managedPid[nrManaged++] = current->pid;
        }
        current = current->next;
    }
    EnvMutexUnlock(&g_processManager.lock);
    return managedPid; // 需由上层释放内存
}

int DestroyProcessManager(void)
{
    EnvMutexDestroy(&g_processManager.lock);
    EnvMutexDestroy(&g_processManager.threadLock);
    (void)memset_s(&g_processManager, sizeof(struct ProcessManager), 0, sizeof(struct ProcessManager));
    return 0;
}

static void SetPidNrPages(ProcessAttr *attr, size_t *nrPages, int len)
{
    attr->walkPage.nrPage = 0;
    for (int i = 0; i < len; i++) {
        attr->walkPage.nrPages[i] = nrPages[i];
        attr->walkPage.nrPage += nrPages[i];
    }
    SMAP_LOGGER_INFO("Pid %d nrPage %llu.", attr->pid, attr->walkPage.nrPage);
}

static void FreePidFreq(struct AccessPidFreq *apf)
{
    for (int i = 0; i < MAX_NODES; i++) {
        free(apf->freq[i]);
        apf->freq[i] = NULL;
        apf->len[i] = 0;
    }
}

static int InitPidFreq(ProcessAttr *attr, struct AccessPidFreq *apf)
{
    int i;

    apf->pid = attr->pid;
    for (i = 0; i < MAX_NODES; i++) {
        apf->freq[i] = NULL;
    }
    for (i = 0; i < MAX_NODES; i++) {
        apf->len[i] = attr->walkPage.nrPages[i];
        if (apf->len[i] == 0) {
            continue;
        }
        /* Use calloc to ensure freq[i] is zeroed */
        apf->freq[i] = calloc(apf->len[i], sizeof(uint16_t));
        if (!apf->freq[i]) {
            SMAP_LOGGER_ERROR("Alloc pid %d data memory failed\n", apf->pid);
            FreePidFreq(apf);
            return -ENOMEM;
        }
    }
    return 0;
}

static int ReadPidFreq(struct AccessPidFreq *apf)
{
    return AccessIoctlReadPidFreq(apf);
}

static int FillPidData(ProcessAttr *attr, struct ProcessMemBitmap *pmb)
{
    int ret;
    struct AccessPidFreq apf = { 0 }; // Make sure apf len and freq is zeroed

    ret = InitPidActcData(attr);
    if (ret) {
        SMAP_LOGGER_ERROR("Init pid %d actc data failed.", attr->pid);
        return ret;
    }
    ret = InitPidFreq(attr, &apf);
    if (ret) {
        SMAP_LOGGER_ERROR("Init pid %d freq failed: %d\n", attr->pid, ret);
        return ret;
    }
    ret = ReadPidFreq(&apf);
    if (ret) {
        SMAP_LOGGER_ERROR("Read pid %d freq failed\n", attr->pid);
        FreePidFreq(&apf);
        return ret;
    }
    ret = FillActcData(attr, pmb, &apf);
    if (ret) {
        SMAP_LOGGER_ERROR("Fill pid %d actc data failed.", attr->pid);
        ResetActcDataForPid(attr);
        FreePidFreq(&apf);
        return ret;
    }
    FreePidFreq(&apf);
    return 0;
}

static int BuildBitmapBuf(size_t *len, char **buf)
{
    char *tmpBuf;
    size_t tmpLen;
    int ret = AccessIoctlWalkPagemap(&tmpLen);
    if (ret) {
        SMAP_LOGGER_ERROR("access ioctl walk pagemap error: %d.", ret);
        return ret;
    }
    SMAP_LOGGER_INFO("AccessIoctlWalkPagemap bufLen %zu.", tmpLen);
    if (tmpLen == 0) {
        SMAP_LOGGER_ERROR("Access ioctl walk pagemap len invalid: %zu.", tmpLen);
        return -EINVAL;
    }

    tmpBuf = malloc(tmpLen);
    if (!tmpBuf) {
        tmpLen = 0;
        return -ENOMEM;
    }

    *len = tmpLen;
    *buf = tmpBuf;

    return 0;
}

static inline void FreePmbData(struct ProcessMemBitmap *pmb)
{
    for (int nid = 0; nid < MAX_NODES; nid++) {
        free(pmb->data[nid]);
        pmb->data[nid] = NULL;
    }
}

static inline void FreeWhiteListBm(struct ProcessMemBitmap *pmb)
{
    for (int nid = 0; nid < MAX_NODES; nid++) {
        free(pmb->whiteListBm[nid]);
        pmb->whiteListBm[nid] = NULL;
    }
}

static int ParseBitmapPid(struct ProcessMemBitmap *pmb, char *buf, size_t *offset)
{
    int ret;
    size_t pidSize = sizeof(pmb->pid);

    ret = memcpy_s(&pmb->pid, pidSize, buf, pidSize);
    if (ret) {
        return -ret;
    }
    *offset += pidSize;
    return 0;
}

static int ParseBitmapNrPages(struct ProcessMemBitmap *pmb, char *buf, size_t *offset)
{
    int ret;
    size_t pageNumSize = sizeof(pmb->nrPages[0]);
    size_t tmpOffset = 0;

    for (int nid = 0; nid < MAX_NODES; nid++) {
        ret = memcpy_s(&pmb->nrPages[nid], pageNumSize, buf + tmpOffset, pageNumSize);
        if (ret) {
            return -ret;
        }
        tmpOffset += pageNumSize;
    }
    *offset += tmpOffset;
    return 0;
}

static int ParseBitmapLen(struct ProcessMemBitmap *pmb, char *buf, size_t *offset)
{
    int ret;
    size_t lenSize = sizeof(pmb->len[0]);
    size_t tmpOffset = 0;

    for (int nid = 0; nid < MAX_NODES; nid++) {
        ret = memcpy_s(&pmb->len[nid], lenSize, buf + tmpOffset, lenSize);
        if (ret) {
            return -ret;
        }
        tmpOffset += lenSize;
        SMAP_LOGGER_DEBUG("pid %d Node%d bmLen %zu.", pmb->pid, nid, pmb->len[nid]);
    }
    *offset += tmpOffset;
    return 0;
}

static int ParseBitmapVmSize(struct ProcessMemBitmap *pmb, char *buf, size_t *offset)
{
    int ret;
    size_t vmSize = sizeof(pmb->vmSize);

    ret = memcpy_s(&pmb->vmSize, vmSize, buf, vmSize);
    if (ret) {
        return -ret;
    }
    *offset += vmSize;
    return 0;
}

static int ParseBitmapData(struct ProcessMemBitmap *pmb, char *buf, size_t *offset)
{
    int ret;
    size_t bitmapSize = sizeof(*pmb->data[0]);
    size_t tmpOffset = 0;

    for (int nid = 0; nid < MAX_NODES; nid++) {
        if (pmb->len[nid] == 0) {
            continue;
        }
        ret = memcpy_s(pmb->data[nid], bitmapSize * pmb->len[nid], buf + tmpOffset, bitmapSize * pmb->len[nid]);
        if (ret) {
            FreePmbData(pmb);
            return -ret;
        }
        tmpOffset += bitmapSize * pmb->len[nid];
    }
    *offset += tmpOffset;
    return 0;
}

static int ParseWhiteListBitmap(struct ProcessMemBitmap *pmb, char *buf, size_t *offset)
{
    int ret;
    size_t bitmapSize = sizeof(*pmb->data[0]);
    size_t tmpOffset = 0;

    for (int nid = 0; nid < MAX_NODES; nid++) {
        if (pmb->len[nid] == 0) {
            continue;
        }
        ret = memcpy_s(pmb->whiteListBm[nid], bitmapSize * pmb->len[nid], buf + tmpOffset, bitmapSize * pmb->len[nid]);
        if (ret) {
            FreePmbData(pmb);
            FreeWhiteListBm(pmb);
            return -ret;
        }
        tmpOffset += bitmapSize * pmb->len[nid];
    }
    *offset += tmpOffset;
    return 0;
}

static int InitPmbData(struct ProcessMemBitmap *pmb)
{
    int nid;
    size_t bitmapSize = sizeof(*pmb->data[0]);

    for (nid = 0; nid < MAX_NODES; nid++) {
        pmb->data[nid] = NULL;
    }
    for (nid = 0; nid < MAX_NODES; nid++) {
        SMAP_LOGGER_DEBUG("Node%d data size %zu.", nid, bitmapSize * pmb->len[nid]);
        if (pmb->len[nid] == 0) {
            continue;
        }
        pmb->data[nid] = malloc(bitmapSize * pmb->len[nid]);
        if (!pmb->data[nid]) {
            FreePmbData(pmb);
            return -ENOMEM;
        }
        pmb->whiteListBm[nid] = malloc(bitmapSize * pmb->len[nid]);
        if (!pmb->whiteListBm[nid]) {
            SMAP_LOGGER_ERROR("pmb whiteListBm[%d] malloc failed.", nid);
            FreePmbData(pmb);
            FreeWhiteListBm(pmb);
            return -ENOMEM;
        }
    }
    return 0;
}

static int ParseBitmap(size_t bufLen, char *buf, size_t *offset, struct ProcessMemBitmap *pmb)
{
    size_t mappingSize = sizeof(*pmb->mapping);
    int nid;
    size_t newOffset = *offset;

    int ret = ParseBitmapPid(pmb, buf + newOffset, &newOffset);
    if (ret) {
        SMAP_LOGGER_ERROR("ParseBitmapPid err: %d.", ret);
        return ret;
    }

    ret = ParseBitmapNrPages(pmb, buf + newOffset, &newOffset);
    if (ret) {
        SMAP_LOGGER_ERROR("ParseBitmapNrPages err: %d.", ret);
        return ret;
    }

    ret = ParseBitmapLen(pmb, buf + newOffset, &newOffset);
    if (ret) {
        SMAP_LOGGER_ERROR("ParseBitmapLen err: %d.", ret);
        return ret;
    }

    ret = ParseBitmapVmSize(pmb, buf + newOffset, &newOffset);
    if (ret) {
        SMAP_LOGGER_ERROR("ParseBitmapVmSize err: %d.", ret);
        return ret;
    }
    if (pmb->vmSize) {
        SMAP_LOGGER_INFO("pid %d vm size %u.", pmb->pid, pmb->vmSize);
    }

    ret = InitPmbData(pmb);
    if (ret) {
        SMAP_LOGGER_ERROR("InitPmbData err: %d.", ret);
        return ret;
    }

    ret = ParseBitmapData(pmb, buf + newOffset, &newOffset);
    if (ret) {
        SMAP_LOGGER_ERROR("ParseBitmapData err: %d.", ret);
        return ret;
    }

    ret = ParseWhiteListBitmap(pmb, buf + newOffset, &newOffset);
    if (ret) {
        SMAP_LOGGER_ERROR("ParseWhiteListBitmap err: %d.", ret);
        return ret;
    }
    if (pmb->vmSize) {
        pmb->mapping = (uint32_t *)((char *)buf + newOffset);
        newOffset += mappingSize * pmb->vmSize;
    }
    SMAP_LOGGER_INFO("read continue %zu %zu.", newOffset, bufLen);

    *offset = newOffset;
    return 0;
}

uint64_t CalcRemoteBorrowPages(uint64_t size)
{
    uint64_t result = size;
    PidType type = GetPidType(&g_processManager);
    if (type == PROCESS_TYPE) {
        result = result << SHIFT_MB_TO_4K;
    } else {
        result = result >> SHIFT_MB_TO_2M;
    }
    return result;
}

static void NoAccountAlloc(int remoteNid, ProcessAttr *attr)
{
    int i;
    int nrLocalNuma = GetNrLocalNuma();
    int l1Nid[nrLocalNuma];
    int l1Len = 0;
    uint32_t tmpUsedTotal = 0;
    double ratio;
    for (i = 0; i < nrLocalNuma; i++) {
        if (InAttrL1(attr, i)) {
            l1Nid[l1Len++] = i;
        }
    }
    if (l1Len != 0 && attr->walkPage.nrPages[remoteNid] != 0) {
        ratio = (double)attr->walkPage.nrPages[remoteNid] / l1Len / attr->walkPage.nrPages[remoteNid];
        for (i = 0; i < l1Len; i++) {
            if (i == l1Len - 1) {
                attr->strategyAttr.allocRemoteNrPages[l1Nid[i]][remoteNid - nrLocalNuma] =
                    attr->walkPage.nrPages[remoteNid] - tmpUsedTotal;
                break;
            }
            uint32_t tmpUsed = attr->walkPage.nrPages[remoteNid] * ratio;
            tmpUsedTotal += tmpUsed;
            attr->strategyAttr.allocRemoteNrPages[l1Nid[i]][remoteNid - nrLocalNuma] = tmpUsed;
            SMAP_LOGGER_DEBUG("NoAccountAlloc pid: %d, allocRemoteNrPages[%d][%d] %u.", attr->pid, l1Nid[i],
                              remoteNid - nrLocalNuma,
                              attr->strategyAttr.allocRemoteNrPages[l1Nid[i]][remoteNid - nrLocalNuma]);
        }
    }
}

static void ClearNormalPidAccount(ProcessAttr *attr, int remoteNode, int nrLocalNuma)
{
    if (attr->state != PROC_MOVE) {
        for (int i = 0; i < nrLocalNuma; i++) {
            attr->strategyAttr.remoteNrPagesAfterMigrate[i][remoteNode] = 0;
        }
    }
}

// returnFlag为true表示该NUMA处理完成，无需后续处理
static void CheckAccountAndNrPage(ProcessAttr *attr, bool returnFlag[REMOTE_NUMA_NUM])
{
    int i, j;
    int nrLocalNuma = GetNrLocalNuma();
    /**
     * 每个远端numa的账本，和nrPage进行对比
     * 1、远端numa有账本，但是nrPage没有，对账本进行清零
     * 2、nrPage有但是远端numa没有账本，
     *   1）检查L1的分布情况，平均分
     *   2）L1没有分布情况，按照CPU绑定情况，平均分
     */
    for (j = 0; j < REMOTE_NUMA_NUM; j++) {
        int remoteNid = nrLocalNuma + j;
        uint32_t tmpTotal = 0;
        double ratio;
        if (NotInAttrL2(attr, remoteNid)) {
            continue;
        }
        if (attr->walkPage.nrPages[remoteNid] == 0) {
            ClearNormalPidAccount(attr, j, nrLocalNuma);
            continue;
        }
        for (i = 0; i < nrLocalNuma; i++) {
            tmpTotal += attr->strategyAttr.remoteNrPagesAfterMigrate[i][j];
        }
        // nrPage有但是远端numa没有账本
        if (tmpTotal == 0) {
            NoAccountAlloc(remoteNid, attr);
            continue;
        }
        returnFlag[j] = false;
    }
}

static void CalNrPagesPerLocalNuma(ProcessAttr *attr)
{
    int nrLocalNuma = GetNrLocalNuma();
    for (int i = 0; i < nrLocalNuma; i++) {
        uint32_t tmpPidNrPagesLocalTotal = 0;
        for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
            tmpPidNrPagesLocalTotal += attr->strategyAttr.allocRemoteNrPages[i][j];
        }
        SMAP_LOGGER_DEBUG("pid: %d, CalNrPagesPerLocalNuma 1 [%d]: %u %u.", attr->pid, i, tmpPidNrPagesLocalTotal,
                          attr->walkPage.nrPages[i]);
        attr->strategyAttr.nrPagesPerLocalNuma[i] = tmpPidNrPagesLocalTotal + attr->walkPage.nrPages[i];
        SMAP_LOGGER_DEBUG("pid: %d, CalNrPagesPerLocalNuma 2 [%d]: %u.", attr->pid, i,
                          attr->strategyAttr.nrPagesPerLocalNuma[i]);
    }
}

static void CalRemotePerLocalWithAccount(int j, ProcessAttr *attr)
{
    int i;
    int nrLocalNuma = GetNrLocalNuma();
    uint32_t tmpRemoteNrPages = 0;
    for (i = 0; i < nrLocalNuma; i++) {
        SMAP_LOGGER_DEBUG("pid: %d, remoteNrPagesAfterMigrate [%d][%d]: %u.", attr->pid, i, j,
                          attr->strategyAttr.remoteNrPagesAfterMigrate[i][j]);
        tmpRemoteNrPages += attr->strategyAttr.remoteNrPagesAfterMigrate[i][j];
    }
    if (tmpRemoteNrPages == 0) {
        return;
    }
    double tmpPairRatio[LOCAL_NUMA_NUM];
    int ratioLen = 0;
    for (i = 0; i < nrLocalNuma; i++) {
        tmpPairRatio[i] = (double)attr->strategyAttr.remoteNrPagesAfterMigrate[i][j] / tmpRemoteNrPages;
        if (tmpPairRatio[i] > 0) {
            ratioLen++;
        }
    }

    uint32_t tmpUsedTotal = 0;
    // 2、Pid远端使用量：pid本地单一numa占比 * PID对应的远端numa数量
    for (i = 0; i < nrLocalNuma && i < LOCAL_NUMA_NUM; i++) {
        if (tmpPairRatio[i] == 0) {
            continue;
        }
        SMAP_LOGGER_DEBUG(
            "CalNrPagesLocalTotalPerPid pid: %d tmp ratio info: i:[%d] j:[%d] nrPage: %u, tmpRatio: %.2lf.", attr->pid,
            i, j, attr->walkPage.nrPages[j + nrLocalNuma], tmpPairRatio[i]);
        if (ratioLen == 1) {
            attr->strategyAttr.allocRemoteNrPages[i][j] = attr->walkPage.nrPages[j + nrLocalNuma] - tmpUsedTotal;
            SMAP_LOGGER_DEBUG("CalNrPagesLocalTotalPerPid pid: %d [%d][%d]: has remote page num: %u.", attr->pid, i, j,
                              attr->strategyAttr.allocRemoteNrPages[i][j]);
            break;
        }
        ratioLen--;
        uint32_t tmpUsed = attr->walkPage.nrPages[j + nrLocalNuma] * tmpPairRatio[i];
        tmpUsedTotal += tmpUsed;
        attr->strategyAttr.allocRemoteNrPages[i][j] = tmpUsed;
        SMAP_LOGGER_DEBUG("CalNrPagesLocalTotalPerPid pid: %d [%d][%d]: has remote page num: %u, tmpUsed: %u.",
                          attr->pid, i, j, attr->strategyAttr.allocRemoteNrPages[i][j], tmpUsed);
    }
}

static void CalRemotePerLocal(ProcessAttr *attr)
{
    int i, j;
    bool returnFlag[REMOTE_NUMA_NUM] = { true };
    // 检查账本和当前内存页分布情况，处理有远端内存，但是没有账本的情况
    CheckAccountAndNrPage(attr, returnFlag);
    int nrLocalNuma = GetNrLocalNuma();

    for (j = 0; j < REMOTE_NUMA_NUM; j++) {
        // 1、Pid本地单一numa占比：pid->本地numa/sum(所有本地numa总迁出量)
        if (NotInAttrL2(attr, nrLocalNuma + j)) {
            continue;
        }
        if (returnFlag[j]) {
            continue;
        }
        // 有账本，并且nrPage[remoteNid] != 0，计算allocRemoteNrPages
        CalRemotePerLocalWithAccount(j, attr);
    }
}

static void CalNrPagesLocalTotalPerPid(ProcessAttr *attr)
{
    uint32_t tmpRemoteNrPages;
    int i, j;

    // 计算每个本地numa，对应可迁出到远端每个numa的内存量
    CalRemotePerLocal(attr);

    // pid本地numa总使用量：Pid本地numa数量+Pid远端使用量
    CalNrPagesPerLocalNuma(attr);
}

static void CalNrPagesLocalTotal(void)
{
    ProcessAttr *attr = g_processManager.processes;
    int ret;

    while (attr) {
        if (IsMultiNumaVm(attr) && GetRunMode() == MEM_POOL_MODE) {
            attr = attr->next;
            continue;
        }
        SMAP_LOGGER_DEBUG("CalNrPagesLocalTotal pid: %d.", attr->pid);
        CalNrPagesLocalTotalPerPid(attr);
        attr = attr->next;
    }
}

// 计算远端内存分配tmpNrPagesToUse下，不同pid应该迁出多少内存，结果叠加在l2RemoteMemRatio中
static void CalRemoteNumaAllocPerPid(int i, int j, uint32_t tmpNrPagesToUse,
                                     uint32_t tmpMaxAllocNrPages[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM])
{
    ProcessAttr *attr = g_processManager.processes;

    PidType type = GetPidType(&g_processManager);
    if (g_processManager.nr[type] == 0) {
        return;
    }
    double tmpRatioPerPid;

    // 再按比例分配tmpNrPagesToUse
    if (tmpMaxAllocNrPages[i][j] == 0) {
        return;
    }
    // 根据比例计算每个PID的迁出比例，更新迁出的比例到l2RemoteMemRatio
    while (attr) {
        // 1）每个PID最大迁出量/总最大迁出量 = 最大迁出量比例
        tmpRatioPerPid =
            (double)attr->strategyAttr.nrPagesPerLocalNuma[i] *
            ((attr->strategyAttr.initRemoteMemRatio[i][j] - attr->strategyAttr.l2RemoteMemRatio[i][j]) / HUNDRED) /
            tmpMaxAllocNrPages[i][j];
        SMAP_LOGGER_DEBUG("CalRemoteNumaAllocPerPid 1: %u [%d][%d]: %.2lf %u.", tmpNrPagesToUse, i, j, tmpRatioPerPid,
                          attr->strategyAttr.nrPagesPerLocalNuma[i]);

        // 2）最大迁出量比例 * numa可用量 = 每个PID可用的量（即numa迁出的ratio ）
        attr->strategyAttr.l2RemoteMemRatio[i][j] +=
            ((double)tmpNrPagesToUse * tmpRatioPerPid / attr->strategyAttr.nrPagesPerLocalNuma[i]) * HUNDRED;
        SMAP_LOGGER_DEBUG("CalRemoteNumaAllocPerPid 2: %.2lf.", attr->strategyAttr.l2RemoteMemRatio[i][j]);

        attr = attr->next;
    }
}

static void CalTmpBorrowPage(uint32_t tmpMaxAllocNrPages[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM],
                             uint32_t tmpPrivateBorrowPageToUse[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM],
                             uint32_t tmpSharedBorrowPageToUse[REMOTE_NUMA_NUM])
{
    struct RemoteNumaInfo remoteNumaInfo = g_processManager.remoteNumaInfo;
    for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
        if (remoteNumaInfo.sharedSize[j] > 0) {
            tmpSharedBorrowPageToUse[j] = CalcRemoteBorrowPages(remoteNumaInfo.sharedSize[j]) -
                                          MIN(CalcRemoteBorrowPages(remoteNumaInfo.sharedSize[j]) * RESERVED_RATIO,
                                              CalcRemoteBorrowPages(RESERVED_MEMORY));
            SMAP_LOGGER_DEBUG("tmpSharedBorrowPageToUse[%d] %llu.", j, tmpSharedBorrowPageToUse[j]);
        }
        for (int i = 0; i < GetNrLocalNuma(); i++) {
            if (tmpMaxAllocNrPages[i][j] == 0) {
                continue;
            }
            if (remoteNumaInfo.privateSize[i][j] > 0) {
                tmpPrivateBorrowPageToUse[i][j] =
                    CalcRemoteBorrowPages(remoteNumaInfo.privateSize[i][j]) -
                    MIN(CalcRemoteBorrowPages(remoteNumaInfo.privateSize[i][j]) * RESERVED_RATIO,
                        CalcRemoteBorrowPages(RESERVED_MEMORY));
                SMAP_LOGGER_DEBUG("tmpPrivateBorrowPageToUse[%d][%d] %llu.", i, j, tmpPrivateBorrowPageToUse[i][j]);
            }
        }
    }
}

static void AllocPrivatePage(uint32_t tmpMaxAllocNrPages[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM],
                             uint32_t tmpPrivateBorrowPageToUse[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM])
{
    for (int i = 0; i < GetNrLocalNuma(); i++) {
        for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
            if (tmpMaxAllocNrPages[i][j] == 0) {
                continue;
            }
            SMAP_LOGGER_DEBUG("tmpMaxAllocNrPages[%d][%d]=%u.", i, j, tmpMaxAllocNrPages[i][j]);
            SMAP_LOGGER_DEBUG("tmpPrivateBorrowPageToUse 2 %llu.", tmpPrivateBorrowPageToUse[i][j]);
            if (tmpPrivateBorrowPageToUse[i][j] == 0) {
                continue;
            }

            uint32_t tmpNrPagesToUse;
            // If 每个numa最大迁出量 > 专属numa：
            if (tmpMaxAllocNrPages[i][j] > tmpPrivateBorrowPageToUse[i][j]) {
                tmpNrPagesToUse = tmpPrivateBorrowPageToUse[i][j];
                CalRemoteNumaAllocPerPid(i, j, tmpNrPagesToUse, tmpMaxAllocNrPages);
                tmpMaxAllocNrPages[i][j] -= tmpPrivateBorrowPageToUse[i][j];
            } else {
                // If 专属numa  > 每个numa最大迁出量：直接迁（迁出的ratio + remote_numa ID）
                tmpNrPagesToUse = tmpMaxAllocNrPages[i][j];
                CalRemoteNumaAllocPerPid(i, j, tmpNrPagesToUse, tmpMaxAllocNrPages);
                tmpMaxAllocNrPages[i][j] = 0;
            }
        }
    }
}

static void AllocBorrowPage(uint32_t tmpMaxAllocNrPages[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM],
                            uint32_t tmpPrivateBorrowPageToUse[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM],
                            uint32_t tmpSharedBorrowPageToUse[REMOTE_NUMA_NUM])
{
    int i, j;
    // 优先使用专属的远端内存
    AllocPrivatePage(tmpMaxAllocNrPages, tmpPrivateBorrowPageToUse);
    double tmpRatioPerLocalNuma[LOCAL_NUMA_NUM];
    // 再使用共享远端内存
    for (j = 0; j < REMOTE_NUMA_NUM; j++) {
        uint32_t tmpNrPagesCanMigOut = 0;
        if (tmpSharedBorrowPageToUse[j] == 0) {
            continue;
        }
        for (i = 0; i < GetNrLocalNuma(); i++) {
            tmpRatioPerLocalNuma[i] = 0;
            tmpNrPagesCanMigOut += tmpMaxAllocNrPages[i][j];
        }
        if (tmpNrPagesCanMigOut == 0) {
            continue;
        }
        for (i = 0; i < GetNrLocalNuma(); i++) {
            tmpRatioPerLocalNuma[i] = (double)tmpMaxAllocNrPages[i][j] / tmpNrPagesCanMigOut;
            // 将共享远端内存，分给每个本地numa去迁出，按照各本地numa可迁出的比例分配
            uint32_t canUsePage = tmpRatioPerLocalNuma[i] * tmpSharedBorrowPageToUse[j];
            SMAP_LOGGER_INFO("tmpRatioPerLocalNuma[%d] %.2lf, tmpNrPagesCanMigOut: %u, SharedBorrow[%d]: %u.", i,
                             tmpRatioPerLocalNuma[i], tmpNrPagesCanMigOut, j, tmpSharedBorrowPageToUse[j]);
            if (canUsePage > tmpMaxAllocNrPages[i][j]) {
                CalRemoteNumaAllocPerPid(i, j, tmpMaxAllocNrPages[i][j], tmpMaxAllocNrPages);
            } else {
                CalRemoteNumaAllocPerPid(i, j, canUsePage, tmpMaxAllocNrPages);
            }
        }
    }
}

static void CalRemoteNumaSizeAllocPerNuma(void)
{
    ProcessAttr *attr = g_processManager.processes;
    struct RemoteNumaInfo remoteNumaInfo = g_processManager.remoteNumaInfo;
    uint32_t tmpMaxAllocNrPages[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM] = { 0 };
    int i, j;

    // 计算每个本地远端对应按照ratio可迁出的最大量
    while (attr) {
        for (i = 0; i < GetNrLocalNuma(); i++) {
            for (j = 0; j < REMOTE_NUMA_NUM; j++) {
                tmpMaxAllocNrPages[i][j] +=
                    attr->strategyAttr.nrPagesPerLocalNuma[i] * attr->strategyAttr.initRemoteMemRatio[i][j] / HUNDRED;
                SMAP_LOGGER_DEBUG("tmpMaxAllocNrPages[%d][%d]=%u, initRemoteMemRatio=%.2lf.", i, j,
                                  tmpMaxAllocNrPages[i][j], attr->strategyAttr.initRemoteMemRatio[i][j]);
                attr->strategyAttr.l2RemoteMemRatio[i][j] = 0;
            }
        }
        attr = attr->next;
    }

    uint32_t tmpPrivateBorrowPageToUse[LOCAL_NUMA_NUM][REMOTE_NUMA_NUM] = { 0 };
    uint32_t tmpSharedBorrowPageToUse[REMOTE_NUMA_NUM] = { 0 };
    // 在远端预留多少内存, 暂不支持混用, 否则预留的内存会比预期多(超过200M)
    CalTmpBorrowPage(tmpMaxAllocNrPages, tmpPrivateBorrowPageToUse, tmpSharedBorrowPageToUse);

    // 用远端借用的内存计算每个pid，每个numa可迁出的比例
    AllocBorrowPage(tmpMaxAllocNrPages, tmpPrivateBorrowPageToUse, tmpSharedBorrowPageToUse);
}

static void CalcMigrateNrPagesPerPIDMuiltNuma(void)
{
    struct RemoteNumaInfo *numaInfo = &g_processManager.remoteNumaInfo;
    PidType type = GetPidType(&g_processManager);
    if (g_processManager.nr[type] == 0) {
        return;
    }
    // 根据账本信息，计算每个PID各本地numa可支配的内存；
    CalNrPagesLocalTotal();

    if (g_runMode == MEM_POOL_MODE) {
        return;
    }
    EnvMutexLock(&numaInfo->lock);
    ClearRemoteMemUsed();
    CalRemoteMemUsed();
    // 按照远端numa的粒度，和上一步计算的迁移量，计算每个本地numa相对于1个远端numa的比例
    CalRemoteNumaSizeAllocPerNuma();
    EnvMutexUnlock(&numaInfo->lock);
}

static int BuildAndFillBitmapBuf(size_t *len, char **buf)
{
    int ret;
    ret = BuildBitmapBuf(len, buf);
    if (ret) {
        SMAP_LOGGER_ERROR("Access ioctl walk pagemap error: %d.", ret);
        return ret;
    }
    SMAP_LOGGER_INFO("Build bitmap buffer done.");
    ret = AccessRead(*len, *buf);
    if (ret) {
        SMAP_LOGGER_ERROR("Access read pagemap error: %d.", ret);
        free(*buf);
        return ret;
    }
    return 0;
}

int BuildAllPidData(void)
{
    int ret, failedCount = 0;
    char *buf;
    size_t bufLen;
    EnvMutexLock(&g_processManager.lock);
    ret = BuildAndFillBitmapBuf(&bufLen, &buf);
    if (ret) {
        SMAP_LOGGER_ERROR("BuildAllPidData: build and fill BitmapBuf error: %d.", ret);
        EnvMutexUnlock(&g_processManager.lock);
        return ret;
    }
    for (size_t offset = 0; offset < bufLen;) {
        struct ProcessMemBitmap pmb = { 0 };
        SMAP_LOGGER_INFO("Parse bitmap from %zu.", offset);
        ret = ParseBitmap(bufLen, buf, &offset, &pmb);
        if (ret < 0) {
            SMAP_LOGGER_ERROR("parse bitmap failed.");
            failedCount++;
            break;
        }
        ProcessAttr *current = GetProcessAttrLocked(pmb.pid);
        if (current && current->scanType == NORMAL_SCAN) {
            SMAP_LOGGER_INFO("Pid %d, numaNodes %#x, nrLocalNuma %u.", current->pid, current->numaAttr.numaNodes,
                             g_processManager.nrLocalNuma);
            SetPidNrPages(current, pmb.nrPages, MAX_NODES);
            ret = FillPidData(current, &pmb);
            if (ret) {
                SMAP_LOGGER_ERROR("Fill pid %d actc data failed.", current->pid);
                failedCount++;
            }
        }
        FreePmbData(&pmb);
        FreeWhiteListBm(&pmb);
    }
    CalcMigrateNrPagesPerPIDMuiltNuma();
    free(buf);
    EnvMutexUnlock(&g_processManager.lock);
    return failedCount;
}

static bool IsInPidArr(pid_t *pidArr, int len, pid_t pid)
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
        if (pidArr[i] == pid) {
            return true;
        }
    }
    return false;
}

struct ProcessManager *GetProcessManager(void)
{
    return &g_processManager;
}

int SetRemoteNumaInfo(int srcNid, int destNid, uint64_t size)
{
    int ret;
    int column = destNid - g_processManager.nrLocalNuma;
    struct RemoteNumaInfo *numaInfo = &g_processManager.remoteNumaInfo;
    EnvMutexLock(&numaInfo->lock);
    ret = SyncOneNumaConfig(srcNid, destNid, size);
    if (ret) {
        SMAP_LOGGER_ERROR("SyncOneNumaConfig %d-%d to %llu failed: %d.", srcNid, destNid, size, ret);
        EnvMutexUnlock(&numaInfo->lock);
        return -EBADF;
    }
    SMAP_LOGGER_INFO("SetRemoteNumaInfo %d-%d to %llu.", srcNid, column, size);
    if (srcNid == NUMA_NO_NODE) {
        numaInfo->sharedSize[column] = size;
    } else {
        numaInfo->privateSize[srcNid][column] = size;
    }
    for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
        numaInfo->usedInfo[j].ifUsedFreshed = false;
        numaInfo->usedInfo[j].size = CalcRemoteBorrowPages(numaInfo->sharedSize[j]);
        SMAP_LOGGER_DEBUG("Node%d shared size: %llu.", j, numaInfo->usedInfo[j].size);
        for (int i = 0; i < g_processManager.nrLocalNuma; i++) {
            numaInfo->usedInfo[j].size += CalcRemoteBorrowPages(numaInfo->privateSize[i][j]);
            numaInfo->privateUsedInfo[i][j].ifUsedFreshed = false;
            numaInfo->privateUsedInfo[i][j].size = CalcRemoteBorrowPages(numaInfo->privateSize[i][j]);
            SMAP_LOGGER_INFO("local %d borrow remote %d private size: %llu.", i, j,
                             numaInfo->privateUsedInfo[i][j].size);
        }
        SMAP_LOGGER_INFO("Node%d total borrow size: %llu.", j, numaInfo->usedInfo[j].size);
    }
    EnvMutexUnlock(&numaInfo->lock);
    return 0;
}

static bool CheckPrivateBorrowUsed(int destNid)
{
    int column = destNid - g_processManager.nrLocalNuma;
    struct RemoteNumaInfo *numaInfo = &g_processManager.remoteNumaInfo;
    bool flag;
    for (int count = 0; count < MAX_FRESH_USED_TIME; count++) {
        flag = false;
        EnvMutexLock(&numaInfo->lock);
        for (int i = 0; i < g_processManager.nrLocalNuma; i++) {
            if (numaInfo->privateUsedInfo[i][column].ifUsedFreshed == false) {
                SMAP_LOGGER_INFO("Private used info not fresh , local: %d, remote nid: %d, used: %d, freshed: %d.", i,
                                 destNid, numaInfo->privateUsedInfo[i][column].used,
                                 numaInfo->privateUsedInfo[i][column].ifUsedFreshed);
                flag = true;
                break;
            }
        }
        if (flag) {
            EnvMutexUnlock(&numaInfo->lock);
            EnvMsleep(WAIT_FRESH_USED_PERIOD);
            continue;
        }
        for (int i = 0; i < g_processManager.nrLocalNuma; i++) {
            SMAP_LOGGER_INFO("CheckPrivateBorrowUsed, remote nid: %d, column: %d, used: %d, size: %d.", destNid, column,
                             numaInfo->privateUsedInfo[i][column].used, numaInfo->privateUsedInfo[i][column].size);
            if (numaInfo->privateUsedInfo[i][column].used > numaInfo->privateUsedInfo[i][column].size) {
                EnvMutexUnlock(&numaInfo->lock);
                return false;
            }
        }
        EnvMutexUnlock(&numaInfo->lock);
        return true;
    }
    return false;
}

static bool CheckBorrowUsed(int destNid)
{
    int column = destNid - g_processManager.nrLocalNuma;
    struct RemoteNumaInfo *numaInfo = &g_processManager.remoteNumaInfo;
    for (int count = 0; count < MAX_FRESH_USED_TIME; count++) {
        EnvMutexLock(&numaInfo->lock);
        if (numaInfo->usedInfo[column].ifUsedFreshed == false) {
            EnvMutexUnlock(&numaInfo->lock);
            EnvMsleep(WAIT_FRESH_USED_PERIOD);
            SMAP_LOGGER_INFO("Remote numa info not fresh , remote nid: %d, used: %d, freshed: %d.", destNid,
                             numaInfo->usedInfo[column].used, numaInfo->usedInfo[column].ifUsedFreshed);
            continue;
        }
        SMAP_LOGGER_INFO("CheckBorrowUsed, remote nid: %d, column: %d, used: %d, size: %d.", destNid, column,
                         numaInfo->usedInfo[column].used, numaInfo->usedInfo[column].size);
        if (numaInfo->usedInfo[column].used > numaInfo->usedInfo[column].size) {
            EnvMutexUnlock(&numaInfo->lock);
            return false;
        }
        EnvMutexUnlock(&numaInfo->lock);
        return true;
    }
    return false;
}

bool CheckReadyMigrateBack(int destNid)
{
    // 如果已经没有管理中的虚机，则默认可以执行迁回
    EnvMutexLock(&g_processManager.lock);
    if (!g_processManager.processes) {
        SMAP_LOGGER_INFO("CheckReadyMigrateBack no process, destNid %d.", destNid);
        EnvMutexUnlock(&g_processManager.lock);
        return true;
    }
    EnvMutexUnlock(&g_processManager.lock);
    struct RemoteNumaInfo *numaInfo = &g_processManager.remoteNumaInfo;
    int column = destNid - g_processManager.nrLocalNuma;

    int nrWait = 0;
    while (nrWait < MAX_MIGRATE_BACK_WAIT_TIME) {
        SMAP_LOGGER_INFO("Wait until ready to migrate back, destNid: %d, nrWait: %d.", destNid, nrWait);
        EnvMutexLock(&numaInfo->lock);
        bool flag = numaInfo->sharedSize[column] > 0;
        EnvMutexUnlock(&numaInfo->lock);
        if (flag) {
            if (CheckBorrowUsed(destNid)) {
                return true;
            }
        } else {
            if (CheckPrivateBorrowUsed(destNid)) {
                return true;
            }
        }
        EnvMsleep(MIGRATE_BACK_CHECK_PERIOD);
        nrWait++;
    }
    SMAP_LOGGER_WARNING("destNid %d not ready to migrate back after %d times.", destNid, MAX_MIGRATE_BACK_WAIT_TIME);
    return false;
}

/*
 * 检查pidArr是否都符合状态切换的要求，会跳过未纳管的pid，不会返回错误
 *
 * 返回值：0-否，1-是，其它-异常
 */
int IsPidArrayStateChangeReady(pid_t *pidArr, int len, int enable)
{
    if (!pidArr) {
        SMAP_LOGGER_ERROR("IsPidArrReadyForChangeStat pidArr is null.");
        return -EINVAL;
    }
    for (int i = 0; i < len; i++) {
        ProcessAttr *attr = GetProcessAttrLocked(pidArr[i]);
        if (!attr) {
            SMAP_LOGGER_INFO("pid %d is not in smap list.", pidArr[i]);
            continue;
        }
        SMAP_LOGGER_DEBUG("pid %d actual state %d.", pidArr[i], attr->state);
        if (enable == DISABLE_PROCESS_MIGRATE && (attr->state != PROC_IDLE && attr->state != PROC_MOVE)) {
            return 0;
        }
        if (enable == ENABLE_PROCESS_MIGRATE && attr->state == PROC_BACK) {
            return 0;
        }
    }
    return 1;
}

/*
 * 检查pidArr是否都处于state，会跳过未纳管的pid，不会返回错误
 *
 * 返回值：0-否，1-是，其它-异常
 */
int IsPidArrInState(pid_t *pidArr, int len, enum ProcessState state)
{
    if (!pidArr) {
        SMAP_LOGGER_ERROR("IsPidArrInState pidArr is null.");
        return -EINVAL;
    }
    for (int i = 0; i < len; i++) {
        ProcessAttr *attr = GetProcessAttrLocked(pidArr[i]);
        if (!attr) {
            SMAP_LOGGER_INFO("pid %d is not in smap list.", pidArr[i]);
            continue;
        }
        SMAP_LOGGER_DEBUG("pid %d actual state %d, expected state %d.", pidArr[i], attr->state, state);
        if (attr->state != state) {
            return 0;
        }
    }
    return 1;
}

static void SetPidArrState(pid_t *pidArr, int len, enum ProcessState state, int enable)
{
    for (int i = 0; i < len; i++) {
        ProcessAttr *attr = GetProcessAttrLocked(pidArr[i]);
        if (!attr) {
            continue;
        }
        /* enable == 1时，迁移状态的pid也视为合理状态，不需要设置为空闲态 */
        if (enable == ENABLE_PROCESS_MIGRATE && attr->state == PROC_MIGRATE) {
            SMAP_LOGGER_DEBUG("pid %d is in PROC_MIGRATE state.", attr->pid);
            continue;
        }
        attr->state = state;
    }
}

/*
 * 检查使用指定l2Node的所有pid是否都处于state态
 *
 * 返回值：false-否，true-是
 */
bool IsAllL2NodePidInState(enum ProcessState state, int l2Node)
{
    EnvMutexLock(&g_processManager.lock);
    for (ProcessAttr *attr = g_processManager.processes; attr; attr = attr->next) {
        if (NotEqualToAttrL2(attr, l2Node)) {
            continue;
        }
        if (attr->state != state) {
            EnvMutexUnlock(&g_processManager.lock);
            return false;
        }
    }
    EnvMutexUnlock(&g_processManager.lock);
    return true;
}


static void SetChangePidRemoteMsgPayload(int srcNid, int destNid, int *i, int maxProcessCnt,
                                         struct AccessAddPidPayload *payload)
{
    for (ProcessAttr *attr = g_processManager.processes; attr && *i < maxProcessCnt; attr = attr->next) {
        if (NotEqualToAttrL2(attr, srcNid)) {
            continue;
        }
        SMAP_LOGGER_INFO("ready to change pid %d L2 from %d to %d.", attr->pid, srcNid, destNid);
        payload[*i].pid = attr->pid;
        payload[*i].numaNodes = attr->numaAttr.numaNodes;
        SetL2ByNid(&payload[*i].numaNodes, destNid);
        payload[*i].scanTime = attr->scanTime;
        payload[*i].duration = attr->duration;
        payload[*i].type = attr->scanType;
        (*i)++;
    }
}

static void ChangePidRemoteMemory(ProcessAttr *attr, int srcNodeIndex, int destNodeIndex, uint64_t memSize, int ratio)
{
    int nrLocalNuma = GetNrLocalNuma();
    int l1node;
    if (GetRunMode() == WATERLINE_MODE) {
        l1node = GetAttrL1(attr);
        if (ratio >= attr->strategyAttr.initRemoteMemRatio[l1node][srcNodeIndex]) {
            ClearNodeBit(&attr->numaAttr.numaNodes, srcNodeIndex + LOCAL_NUMA_BITS);
        }
        for (int i = 0; i < g_processManager.nrLocalNuma; i++) {
            attr->strategyAttr.initRemoteMemRatio[i][destNodeIndex] += ratio;
            attr->strategyAttr.initRemoteMemRatio[i][srcNodeIndex] -= ratio;
        }
    } else if (GetRunMode() == MEM_POOL_MODE) {
        uint64_t srcMemSize = 0;
        int remoteNidIndex;
        for (int i = 0; i < attr->remoteNumaCnt; i++) {
            int srcNid = srcNodeIndex + nrLocalNuma;
            if (srcNid == attr->migrateParam[i].nid) {
                srcMemSize = attr->migrateParam[i].memSize;
                remoteNidIndex = i;
                break;
            }
        }
        if (memSize >= srcMemSize) {
            ClearNodeBit(&attr->numaAttr.numaNodes, srcNodeIndex + LOCAL_NUMA_BITS);
            attr->migrateParam[remoteNidIndex].nid = 0;
        }
        attr->migrateParam[remoteNidIndex].memSize -= memSize;

        for (int i = 0; i < g_processManager.nrLocalNuma; i++) {
            attr->strategyAttr.memSize[i][destNodeIndex] += memSize;
            attr->strategyAttr.memSize[i][srcNodeIndex] -= memSize;
        }
    }

    AddAttrL2(attr, destNodeIndex + nrLocalNuma);
    attr->remoteNumaCnt = GetL2Count(attr->numaAttr.numaNodes);
    attr->migrateParam[attr->remoteNumaCnt].nid = destNodeIndex + nrLocalNuma;
    attr->migrateParam[attr->remoteNumaCnt].memSize += memSize;
    SMAP_LOGGER_INFO("========= remoteNumaCnt %d", attr->remoteNumaCnt);
}

static void ChangePidRemoteMemoryByNuma(ProcessAttr *attr, int srcNode, int destNode)
{
    if (GetRunMode() == WATERLINE_MODE) {
        for (int i = 0; i < g_processManager.nrLocalNuma; i++) {
            attr->strategyAttr.initRemoteMemRatio[i][destNode] = attr->strategyAttr.initRemoteMemRatio[i][srcNode];
            attr->strategyAttr.initRemoteMemRatio[i][srcNode] = 0;
        }
    } else if (GetRunMode() == MEM_POOL_MODE) {
        for (int i = 0; i < g_processManager.nrLocalNuma; i++) {
            attr->strategyAttr.memSize[i][destNode] = attr->strategyAttr.memSize[i][srcNode];
            attr->strategyAttr.memSize[i][srcNode] = 0;
        }
    }
}

int ChangePidRemoteByNuma(int srcNid, int destNid)
{
    int i = 0;
    int maxProcessCnt = GetCurrentMaxNrPid();
    int srcNode = srcNid - g_processManager.nrLocalNuma;
    int destNode = destNid - g_processManager.nrLocalNuma;
    ProcessAttr *attr;
    struct AccessAddPidPayload *payload = malloc(sizeof(struct AccessAddPidPayload) * maxProcessCnt);
    if (!payload) {
        SMAP_LOGGER_ERROR("ChangePidRemoteByNuma malloc payload failed.");
        return -ENOMEM;
    }

    EnvMutexLock(&g_processManager.lock);
    SetChangePidRemoteMsgPayload(srcNid, destNid, &i, maxProcessCnt, payload);
    if (i == 0) {
        SMAP_LOGGER_INFO("ChangePidRemoteByNuma len: %d, no need to change.", i);
        EnvMutexUnlock(&g_processManager.lock);
        free(payload);
        return 0;
    }
    SMAP_LOGGER_INFO("ChangePidRemoteByNuma ioctl begin, len: %d.", i);
    int ret = AccessIoctlAddPid(i, payload);
    free(payload);
    if (ret) {
        SMAP_LOGGER_ERROR("ChangePidRemoteByNuma ioctl failed: %d.", ret);
        EnvMutexUnlock(&g_processManager.lock);
        return ret;
    }
    for (attr = g_processManager.processes; attr; attr = attr->next) {
        if (NotEqualToAttrL2(attr, srcNid)) {
            continue;
        }
        SMAP_LOGGER_INFO("change pid %d L2 from %d to %d.", attr->pid, srcNid, destNid);
        for (int j = 0; j < g_processManager.nrLocalNuma; j++) {
            attr->strategyAttr.remoteNrPagesAfterMigrate[j][destNode] +=
                attr->strategyAttr.remoteNrPagesAfterMigrate[j][srcNode];
            attr->strategyAttr.remoteNrPagesAfterMigrate[j][srcNode] = 0;
        }
        ChangePidRemoteMemoryByNuma(attr, srcNode, destNode);
        SetAttrL2(attr, destNid);
    }
    ret = SyncAllProcessConfig();
    if (ret) {
        SMAP_LOGGER_WARNING("Synchronize pid after change remote maybe failed: %d.", ret);
    }
    EnvMutexUnlock(&g_processManager.lock);
    return 0;
}

int EnableProcessMigrate(pid_t *pidArr, int len, int enable)
{
    int retry = WAIT_PROC_STATE_MAX_RETRY;
    enum ProcessState newState;
    newState = enable == ENABLE_PROCESS_MIGRATE ? PROC_IDLE : PROC_MOVE;

    SMAP_LOGGER_DEBUG("enter EnableProcessMigrate.");
    while (true) {
        EnvMutexLock(&g_processManager.lock);
        int ret = IsPidArrayStateChangeReady(pidArr, len, enable);
        if (ret == 1) {
            if (enable == ENABLE_PROCESS_MIGRATE) {
                SMAP_LOGGER_INFO("set pids state to migrate state: %d or %d succeed.", PROC_IDLE, PROC_MIGRATE);
            } else {
                SMAP_LOGGER_INFO("set pids state from %d to %d succeed.", PROC_IDLE, PROC_MOVE);
            }
            SetPidArrState(pidArr, len, newState, enable);
            ret = SyncAllProcessConfig();
            if (ret) {
                SMAP_LOGGER_WARNING("Synchronize pid state maybe failed: %d.", ret);
            }
            EnvMutexUnlock(&g_processManager.lock);
            return 0;
        }
        EnvMutexUnlock(&g_processManager.lock);
        if (ret < 0) {
            SMAP_LOGGER_ERROR("check pid state err: %d.", ret);
            return ret;
        }
        if (--retry < 0) {
            SMAP_LOGGER_INFO("wait for pid state to change timed out, enable: %d.", enable);
            return -ETIMEDOUT;
        }
        SMAP_LOGGER_INFO("wait for pid state to change, %d more times left.", retry);
        EnvMsleep(WAIT_PROC_STATE_PERIOD);
    }
}

/*
 * 检查远端NUMA上的内存是否可被迁回
 *
 * 传入的nid必须是远端NUMA，如果有使用该NUMA的进程是PROC_MOVE状态，则不可执行迁回
 * 返回值：0-否，1-是，其它-异常
 */
int IsRemoteNumaMigrateBackAllowed(int nid)
{
    if (nid < g_processManager.nrLocalNuma) {
        return -EINVAL;
    }
    EnvMutexLock(&g_processManager.lock);
    for (ProcessAttr *attr = g_processManager.processes; attr; attr = attr->next) {
        if (NotEqualToAttrL2(attr, nid)) {
            continue;
        }
        SMAP_LOGGER_DEBUG("pid %d state: %d.", attr->pid, attr->state);
        if (attr->state == PROC_MOVE) {
            SMAP_LOGGER_INFO("pid %d state %d == PROC_MOVE.", attr->pid, attr->state);
            EnvMutexUnlock(&g_processManager.lock);
            return 0;
        }
    }
    EnvMutexUnlock(&g_processManager.lock);
    return 1;
}

/*
 * 检查远端NUMA上的内存是否可被搬移
 *
 * 和IsNumaMigrateBackAllowed相反，如果有使用该NUMA的进程不是PROC_MOVE状态，则不可执行搬移
 * 返回值：0-否，1-是，其它-异常
 */
int IsRemoteNumaMoveAllowed(int nid)
{
    if (nid < g_processManager.nrLocalNuma) {
        return -EINVAL;
    }
    EnvMutexLock(&g_processManager.lock);
    for (ProcessAttr *attr = g_processManager.processes; attr; attr = attr->next) {
        if (NotEqualToAttrL2(attr, nid)) {
            continue;
        }
        SMAP_LOGGER_DEBUG("pid %d state: %d.", attr->pid, attr->state);
        if (attr->state != PROC_MOVE) {
            SMAP_LOGGER_INFO("pid %d state %d != PROC_MOVE.", attr->pid, attr->state);
            EnvMutexUnlock(&g_processManager.lock);
            return 0;
        }
    }
    EnvMutexUnlock(&g_processManager.lock);
    return 1;
}

bool MigOutIsDone(ProcessAttr *attr, bool *isMultiNumaPid)
{
    bool ret = false;
    uint64_t remoteNum;
    pid_t pid = attr->pid;

    attr->enableSwap = false;
    if (IsMultiNumaVm(attr)) {
        *isMultiNumaPid = true;
        for (int i = 0; i < attr->remoteNumaCnt; i++) {
            int l2node = attr->migrateParam[i].nid;
            remoteNum = KBTo2M(attr->migrateParam[i].memSize);
            SMAP_LOGGER_INFO("Pid: %d, l2node: %d, remoteNum: %lu, actcLen: %lu.", pid, l2node, remoteNum,
                             attr->scanAttr.actcLen[l2node]);
            if (attr->scanAttr.actcLen[l2node] != remoteNum) {
                return false;
            }
        }
        attr->enableSwap = true;
        ret = true;
    } else {
        int l1Node = GetAttrL1(attr);
        int l2Node = GetAttrL2(attr) - g_processManager.nrLocalNuma;
        if (l1Node < 0 || l2Node < 0) {
            SMAP_LOGGER_ERROR("Invalid l1Node %d l2Node %d of pid %d.", l1Node, l2Node, pid);
            return false;
        }
        remoteNum = KBTo2M(attr->strategyAttr.memSize[l1Node][l2Node]);
        if (remoteNum > attr->walkPage.nrPage) {
            SMAP_LOGGER_WARNING("Pid %d mig memSize is larger than nrPage.", attr->pid);
        }
        uint64_t localNum = remoteNum >= attr->walkPage.nrPage ? 0 : (attr->walkPage.nrPage - remoteNum);
        SMAP_LOGGER_INFO("localNum %llu, attr->nrPages[l1Node] %llu.", localNum, attr->walkPage.nrPages[l1Node]);
        if (attr->walkPage.nrPage && localNum == attr->walkPage.nrPages[l1Node]) {
            attr->enableSwap = true;
            ret = true;
        }
    }

    return ret;
}

static void SetPayloadValue(struct AccessAddPidPayload *payload, struct MigPidRemoteNumaIoctlMsg *msg, int len)
{
    int runMode = GetRunMode();
    uint64_t srcMemSize;
    int l1node;
    int l2node;
    int nrLocalNuma = GetNrLocalNuma();
    for (int i = 0; i < len; i++) {
        ProcessAttr *attr = GetProcessAttrLocked(msg->payloads[i].pid);
        if (!attr) {
            SMAP_LOGGER_ERROR("GetProcessAttrLocked pid %d null.", msg->payloads[i].pid);
            continue;
        }
        payload[i].pid = attr->pid;
        payload[i].numaNodes = attr->numaAttr.numaNodes;
        l1node = GetAttrL1(attr);
        l2node = msg->payloads[i].srcNid;
        // 远端单numa->远端多numa，使用AddL2ByNid
        if (runMode == WATERLINE_MODE) {
            if (msg->payloads[i].ratio >=
                attr->strategyAttr.initRemoteMemRatio[l1node][l2node - nrLocalNuma]) {
                ClearNodeBit(&payload[i].numaNodes, l2node + (LOCAL_NUMA_BITS - nrLocalNuma));
            }
        } else { // MEM_POOL_MODE
            if (msg->payloads[i].memSize >= attr->strategyAttr.memSize[l1node][l2node - nrLocalNuma]) {
                ClearNodeBit(&payload[i].numaNodes, l2node + (LOCAL_NUMA_BITS - nrLocalNuma));
            }
        }

        AddL2ByNid(&payload[i].numaNodes, msg->payloads[i].destNid);
        payload[i].scanTime = attr->scanTime;
        payload[i].duration = attr->duration;
        payload[i].type = attr->scanType;
    }
}

int ChangePidRemoteByPid(struct MigPidRemoteNumaIoctlMsg *msg)
{
    int maxProcessCnt = GetCurrentMaxNrPid();
    if (!msg || !msg->payloads || !msg->migResArray || msg->pidCnt <= 0 || msg->pidCnt > maxProcessCnt) {
        SMAP_LOGGER_ERROR("ChangePidRemoteByPid msg invalid.");
        return -EINVAL;
    }

    struct AccessAddPidPayload *payload = malloc(sizeof(struct AccessAddPidPayload) * maxProcessCnt);
    if (!payload) {
        SMAP_LOGGER_ERROR("ChangePidRemoteByPid malloc payload failed.");
        return -ENOMEM;
    }

    EnvMutexLock(&g_processManager.lock);
    SetPayloadValue(payload, msg, msg->pidCnt);
    SMAP_LOGGER_INFO("ChangePidRemoteByPid ioctl begin, len: %d.", msg->pidCnt);
    int ret = AccessIoctlAddPid(msg->pidCnt, payload);
    free(payload);
    if (ret) {
        SMAP_LOGGER_ERROR("ChangePidRemoteByNuma ioctl failed: %d.", ret);
        EnvMutexUnlock(&g_processManager.lock);
        return ret;
    }
    SMAP_LOGGER_INFO("ChangePidRemoteByNuma ioctl done.");
    for (int i = 0; i < msg->pidCnt; i++) {
        ProcessAttr *attr = GetProcessAttrLocked(msg->payloads[i].pid);
        if (!attr) {
            continue;
        }
        int srcNode = msg->payloads[i].srcNid - g_processManager.nrLocalNuma;
        int destNode = msg->payloads[i].destNid - g_processManager.nrLocalNuma;
        SMAP_LOGGER_INFO("change pid %d L2 from %d to %d.", attr->pid,
            msg->payloads[i].srcNid, msg->payloads[i].destNid);
        if (GetL1Count(attr->numaAttr.numaNodes) > 1) { // 容器本地多numa
            for (int j = 0; j < g_processManager.nrLocalNuma; j++) {
                attr->strategyAttr.remoteNrPagesAfterMigrate[j][destNode] +=
                    attr->strategyAttr.remoteNrPagesAfterMigrate[j][srcNode];
                attr->strategyAttr.remoteNrPagesAfterMigrate[j][srcNode] = 0;
            }
        } else {
            int l1node = GetAttrL1(attr);
            attr->strategyAttr.remoteNrPagesAfterMigrate[l1node][destNode] += msg->payloads[i].successCnt;
            attr->strategyAttr.remoteNrPagesAfterMigrate[l1node][srcNode] -= msg->payloads[i].successCnt;
        }

        ChangePidRemoteMemory(attr, srcNode, destNode, msg->payloads[i].memSize, msg->payloads[i].ratio);
    }
    ret = SyncAllProcessConfig();
    if (ret) {
        SMAP_LOGGER_WARNING("Synchronize pid after change remote maybe failed: %d.", ret);
    }
    EnvMutexUnlock(&g_processManager.lock);
    return 0;
}

bool IsMemoryLow(pid_t pid)
{
    bool isLow = false;
    EnvMutexLock(&g_processManager.lock);
    ProcessAttr *process = GetProcessAttrLocked(pid);
    if (process && process->isLowMem) {
        SMAP_LOGGER_INFO("Pid %d dest nid memory is low.", pid);
        isLow = true;
    }
    EnvMutexUnlock(&g_processManager.lock);
    return isLow;
}
