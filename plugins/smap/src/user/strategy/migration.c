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
#include <sys/param.h>
#include <sys/time.h>
#include <math.h>
#include "manage/access_ioctl.h"
#include "smap_user_log.h"
#include "advanced-strategy/scene.h"
#include "manage/manage.h"
#include "manage/device.h"
#include "manage/thread.h"
#include "manage/access_ioctl.h"
#include "manage/smap_ioctl.h"
#include "manage/device.h"
#include "strategy.h"
#include "period_config.h"
#include "securec.h"
#include "smap_env.h"
#include "migration.h"

#define MAX_MIG_ADDR_PRINT_LEN 2

int AddMigList(struct MigrateMsg *mMsg, struct MigList *mList)
{
    int i;
    ssize_t j;
    uint64_t oldNum, migrateNum, freePageNum;
    if (!mMsg || !mList) {
        return 0;
    }
    if (mList->nr == 0 || mList->addr == NULL) {
        return 0;
    }
    freePageNum = IsHugeMode() ? GetNrFreeHugePagesByNode(mList->to) : GetNrFreePagesByNode(mList->to);
    if (freePageNum == 0) {
        return 0;
    }

    migrateNum = MIN(mList->nr, freePageNum);
    SMAP_LOGGER_DEBUG("mList->nr: %lu, migrateNum: %lu.", mList->nr, migrateNum);
    if (migrateNum < mList->nr) {
        SMAP_LOGGER_INFO("Change migrate num from %llu to %llu.", mList->nr, migrateNum);
    }
    mMsg->migList[mMsg->cnt].addr = malloc(sizeof(uint64_t) * migrateNum);
    if (!mMsg->migList[mMsg->cnt].addr) {
        SMAP_LOGGER_ERROR("migList->addr malloc failed.");
        return -ENOMEM;
    }
    mMsg->migList[mMsg->cnt].from = mList->from;
    mMsg->migList[mMsg->cnt].to = mList->to;
    mMsg->migList[mMsg->cnt].pid = mList->pid;
    mMsg->migList[mMsg->cnt].nr = migrateNum;
    for (i = 0; i < migrateNum; i++) {
        mMsg->migList[mMsg->cnt].addr[i] = mList->addr[i];
    }
    mMsg->cnt++;
    return 0;
}

static void FreeMigList(struct MigList mList[MAX_NODES][MAX_NODES])
{
    for (int from = 0; from < MAX_NODES; from++) {
        for (int to = 0; to < MAX_NODES; to++) {
            if (mList[from][to].addr) {
                free(mList[from][to].addr);
                mList[from][to].addr = NULL;
            }
        }
    }
}

static void InitMigList(struct MigList mList[MAX_NODES][MAX_NODES], int pid)
{
    for (int from = 0; from < MAX_NODES; from++) {
        for (int to = 0; to < MAX_NODES; to++) {
            mList[from][to].nr = 0;
            mList[from][to].pid = pid;
            mList[from][to].from = from;
            mList[from][to].to = to;
            mList[from][to].addr = NULL;
        }
    }
}

static int BuildMigrationMsg(ProcessAttr *process, struct MigrateMsg *mMsg, uint64_t *migratePage)
{
    int ret = CheckActcDataValid(process);
    if (ret) {
        return ret;
    }
    int l2Node = GetAttrL2(process);
    if (IsNodeForbidden(l2Node)) {
        SMAP_LOGGER_INFO("L2 node%d is forbiddened, pid %d stops migrate out.", l2Node, process->pid);
        return -EPERM;
    }
    if (!migratePage) {
        SMAP_LOGGER_ERROR("migratePage is null.");
        return -EINVAL;
    }
    struct MigList migList[MAX_NODES][MAX_NODES];
    InitMigList(migList, process->pid);
    ret = RunStrategy(process, migList, MAX_NODES);
    if (ret) {
        SMAP_LOGGER_ERROR("Run strategy for pid %d failed: %d.", process->pid, ret);
        FreeMigList(migList);
        return ret;
    }

    uint64_t nrMigTotal = 0;
    for (int from = 0; from < MAX_NODES; from++) {
        for (int to = 0; to < MAX_NODES; to++) {
            if (!migList[from][to].nr) {
                continue;
            }
            nrMigTotal += migList[from][to].nr;
            ret = AddMigList(mMsg, &migList[from][to]);
            if (ret) {
                SMAP_LOGGER_ERROR("Pid %d AddMigList %d %d failed: %d.", process->pid, from, to, ret);
                FreeMigList(migList);
                return ret;
            }
            SMAP_LOGGER_INFO("Numa %d --> Numa %d, mig %d pages.", from, to, migList[from][to].nr);
        }
    }
    FreeMigList(migList);
    *migratePage = *migratePage + nrMigTotal;
    SMAP_LOGGER_DEBUG("Pid %d migList len %llu.", process->pid, nrMigTotal);
    return 0;
}

void UpdateMigResult(struct MigrateMsg *mMsg, struct ProcessManager *manager)
{
    ProcessAttr *current;
    uint64_t successMigCount;
    int fromNid;
    int toNid;
    for (int i = 0; i < mMsg->cnt; i++) {
        current = GetProcessAttrLocked(mMsg->migList[i].pid);
        if (!current || !mMsg->migList[i].successToUser) {
            continue;
        }

        fromNid = mMsg->migList[i].from;
        toNid = mMsg->migList[i].to;
        successMigCount = mMsg->migList[i].nr - mMsg->migList[i].failedMigNr;

        if (toNid >= manager->nrLocalNuma) {
            toNid = mMsg->migList[i].to - manager->nrLocalNuma;
            current->strategyAttr.remoteNrPagesAfterMigrate[fromNid][toNid] += successMigCount;
        } else {
            fromNid = mMsg->migList[i].from - manager->nrLocalNuma;
            if (successMigCount > current->strategyAttr.remoteNrPagesAfterMigrate[toNid][fromNid]) {
                current->strategyAttr.remoteNrPagesAfterMigrate[toNid][fromNid] = 0;
                SMAP_LOGGER_DEBUG("Mig Pages Num Exception: pid %d from %d to %d nr %llu pages", current->pid,
                                  mMsg->migList[i].from, mMsg->migList[i].to, successMigCount);
                continue;
            }
            current->strategyAttr.remoteNrPagesAfterMigrate[toNid][fromNid] -= successMigCount;
        }
        SMAP_LOGGER_INFO("pid %d from %d to %d nr %llu failed_mig_nr %llu success_mig_nr %llu.", mMsg->migList[i].pid,
                         mMsg->migList[i].from, mMsg->migList[i].to, mMsg->migList[i].nr, mMsg->migList[i].failedMigNr,
                         successMigCount);
    }
}

int DoMigration(struct MigrateMsg *mMsg, struct ProcessManager *manager)
{
    int err = 0;
    SMAP_LOGGER_DEBUG("mMsg->cnt %d.", mMsg->cnt);

    uint64_t **tmpAddr = malloc(sizeof(*tmpAddr) * mMsg->cnt);
    if (!tmpAddr) {
        SMAP_LOGGER_ERROR("malloc tmp addr failed.");
        return -ENOMEM;
    }
    for (int i = 0; i < mMsg->cnt; i++) {
        SMAP_LOGGER_DEBUG("nr is %d, from is %d, to is %d, addr list :.", mMsg->migList[i].nr, mMsg->migList[i].from,
                          mMsg->migList[i].to);
        tmpAddr[i] = mMsg->migList[i].addr;
        for (int j = 0; j < mMsg->migList[i].nr; j++) {
            SMAP_LOGGER_DEBUG("%#llx .", mMsg->migList[i].addr[j]);
            if (j >= MAX_MIG_ADDR_PRINT_LEN) {
                SMAP_LOGGER_DEBUG(" ... .");
                break;
            }
        }
    }
    if (mMsg->cnt > 0) {
        err = ioctl(manager->fds.migrate, SMAP_MIG_MIGRATE, mMsg);
    }
    for (int i = 0; i < mMsg->cnt; i++) {
        if (tmpAddr[i]) {
            free(tmpAddr[i]);
        }
    }
    free(tmpAddr);
    return err;
}

static int InitMigrateMsg(struct MigrateMsg *mMsg, struct ProcessManager *manager)
{
    PidType type = GetPidType(manager);
    // 按照pid粒度进行迁移，申请的迁移数组大小为pid的数量*2
    int maxProcessCnt = GetCurrentMaxNrPid();
    int maxPathNum = maxProcessCnt * MAX_PER_PID_MIG_LIST_COUNT;
    mMsg->cnt = 0;
    mMsg->migList = calloc(maxPathNum, sizeof(struct MigList));
    if (!mMsg->migList) {
        SMAP_LOGGER_ERROR("mMsg->migList malloc failed.");
        return -ENOMEM;
    }
    mMsg->mulMig.nrThread = 1;
    mMsg->mulMig.isMulThread = false;
    mMsg->mulMig.pageSize = manager->tracking.pageSize;
    return 0;
}

static int CleanStrategyAttribute(struct ProcessManager *manager)
{
    ProcessAttr *current;
    EnvMutexLock(&manager->lock);
    for (current = manager->processes; current; current = current->next) {
        int ret;
        StrategyAttribute *strAttr = &current->strategyAttr;
        for (int i = 0; i < LOCAL_NUMA_NUM; i++) {
            ret = memset_s(strAttr->l3RemoteMemRatio[i], sizeof(strAttr->l3RemoteMemRatio[0]), 0,
                           sizeof(strAttr->l3RemoteMemRatio[0]));
            if (ret != EOK) {
                EnvMutexUnlock(&manager->lock);
                return ret;
            }
            ret = memset_s(strAttr->allocRemoteNrPages[i], sizeof(strAttr->allocRemoteNrPages[0]), 0,
                           sizeof(strAttr->allocRemoteNrPages[0]));
            if (ret != EOK) {
                EnvMutexUnlock(&manager->lock);
                return ret;
            }
        }
        ret = memset_s(strAttr->nrPagesPerLocalNuma, sizeof(strAttr->nrPagesPerLocalNuma), 0,
                       sizeof(strAttr->nrPagesPerLocalNuma));
        if (ret != EOK) {
            EnvMutexUnlock(&manager->lock);
            return ret;
        }
        for (int i = 0; i < MAX_NODES; i++) {
            ret = memset_s(strAttr->nrMigratePages[i], sizeof(strAttr->nrMigratePages[0]), 0,
                           sizeof(strAttr->nrMigratePages[0]));
            if (ret != EOK) {
                EnvMutexUnlock(&manager->lock);
                return ret;
            }
        }
    }
    EnvMutexUnlock(&manager->lock);
    return 0;
}

static int PerformMigrationPreparation(struct ProcessManager *manager)
{
    int ret = 0;
    int isRamChanged = 0;
    ProcessAttr *current = manager->processes;

    ret = GetRamIsChange(manager, &isRamChanged);
    if (ret) {
        SMAP_LOGGER_ERROR("Get ram change failed! ret: %d.", ret);
        return ret;
    }
    if (isRamChanged) {
        return -EBUSY;
    }
    if (!current) {
        return -EINVAL;
    }
    ret = CleanStrategyAttribute(manager);
    if (ret) {
        SMAP_LOGGER_ERROR("Clean StrategyAttribute failed! ret: %d.", ret);
        return ret;
    }
    ret = BuildAllPidData();
    if (ret) {
        SMAP_LOGGER_ERROR("Build pid data failed! nums:%d.", ret);
    }
    return ret;
}

static int SetMigrateThreadNum(struct MigrateMsg *mMsg, uint64_t migratePages, bool isForcedSingleThread)
{
    if (!mMsg) {
        return -EINVAL;
    }
    if (mMsg->mulMig.pageSize == PAGESIZE_2M && migratePages > LESS_MIG_OUT_2M_PAGE_THRE) {
        mMsg->mulMig.isMulThread = true;
        if (migratePages <= MORE_MIG_OUT_2M_PAGE_THRE) {
            mMsg->mulMig.nrThread = LESS_THREAD_MIG_OUT;
        } else {
            mMsg->mulMig.nrThread = MORE_THREAD_MIG_OUT;
        }
    } else {
        mMsg->mulMig.isMulThread = false;
        mMsg->mulMig.nrThread = SIG_THREAD_MIG_OUT;
    }
    if (migratePages) {
        SMAP_LOGGER_INFO("As for %d pages, set %d migration thread(s).", migratePages, mMsg->mulMig.nrThread);
    } else {
        SMAP_LOGGER_INFO("As for 0 pages, no migration.");
    }
    return 0;
}

static inline long CalcDurationUs(struct timeval start, struct timeval end)
{
    long seconds, micros;

    seconds = end.tv_sec - start.tv_sec;
    micros = end.tv_usec - start.tv_usec;
    return seconds * US_PER_SEC + micros;
}

static void PrintMigSpeed(struct ProcessManager *manager, uint64_t nr, struct timeval start, struct timeval end)
{
    double timeUsed, migSpeed;
    double amount;
    long temp;

    if (nr == 0) {
        return;
    }

    if (GetPidType(manager) == VM_TYPE) {
        amount = (double)nr * PAGESIZE_2M / GIB;
    } else {
        amount = (double)nr * PAGESIZE_4K / GIB;
    }
    temp = CalcDurationUs(start, end);
    if (temp == 0) {
        return;
    }
    timeUsed = (double)temp;
    migSpeed = amount / timeUsed * US_PER_SEC;
    SMAP_LOGGER_DEBUG("Migrate %llu pages, used %.0lfus, speed %.2lfGB/sec.", nr, timeUsed, migSpeed);
}

static void CalProcessNuma(StrategyAttribute *strategyAttr)
{
    int nrLocalNuma = GetNrLocalNuma();
    for (int i = 0; i < nrLocalNuma; i++) {
        if (strategyAttr->nrPagesPerLocalNuma[i] == 0) {
            continue;
        }
        for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
            double resRatio = ((double)strategyAttr->allocRemoteNrPages[i][j] / strategyAttr->nrPagesPerLocalNuma[i]) -
                              (strategyAttr->l3RemoteMemRatio[i][j] / HUNDRED);
            int32_t tmp = ceil(strategyAttr->nrPagesPerLocalNuma[i] * resRatio);
            SMAP_LOGGER_DEBUG("CalProcessNuma l3Ratio[%d][%d]: %.2lf, resRatio: %.2lf, tmp: %d.", i, j,
                              strategyAttr->l3RemoteMemRatio[i][j], resRatio, tmp);

            if (tmp > 0) {
                strategyAttr->nrMigratePages[j + nrLocalNuma][i] = tmp;
                SMAP_LOGGER_DEBUG("CalProcessNuma tmp > 0[%d][%d]: %u.", j + nrLocalNuma, i,
                                  strategyAttr->nrMigratePages[j + nrLocalNuma][i]);
            }
            if (tmp < 0) {
                strategyAttr->nrMigratePages[i][j + nrLocalNuma] = (-tmp);
                SMAP_LOGGER_DEBUG("CalProcessNuma tmp < 0[%d][%d]:  %u.", i, j + nrLocalNuma,
                                  strategyAttr->nrMigratePages[i][j + nrLocalNuma]);
            }
        }
    }
}

typedef struct {
    int numaId;
    int amount;
} NumaMemReduce;

#define MAX_MIG_REDUCE_NUMA 20

int CompareMigIn(const void *a, const void *b)
{
    return ((NumaMemReduce *)a)->amount - ((NumaMemReduce *)b)->amount;
}

int CompareMigOut(const void *b, const void *a)
{
    return ((NumaMemReduce *)a)->amount - ((NumaMemReduce *)b)->amount;
}

static void NumaSwapReduce(StrategyAttribute *strategyAttr, int32_t *numaMemSwap)
{
    NumaMemReduce migInNum[MAX_MIG_REDUCE_NUMA];
    NumaMemReduce migOutNum[MAX_MIG_REDUCE_NUMA];
    int migInCount = 0;
    int migOutCount = 0;
    int transactionCount = 0;

    for (int i = 0; i < (GetNrLocalNuma() + REMOTE_NUMA_NUM); i++) {
        if (numaMemSwap[i] > 0) {
            migInNum[migInCount].numaId = i;
            migInNum[migInCount].amount = numaMemSwap[i];
            migInCount++;
        } else if (numaMemSwap[i] < 0) {
            migOutNum[migOutCount].numaId = i;
            migOutNum[migOutCount].amount = numaMemSwap[i];
            migOutCount++;
        }
    }

    qsort(migInNum, migInCount, sizeof(NumaMemReduce), CompareMigIn);
    qsort(migOutNum, migOutCount, sizeof(NumaMemReduce), CompareMigOut);

    int migInIdx = 0;
    int migOutIdx = 0;
    while (migInIdx < migInCount && migOutIdx < migOutCount) {
        int migInAmt = migInNum[migInIdx].amount;
        int migOutAmt = -migOutNum[migOutIdx].amount;

        int transfer = (migInAmt < migOutAmt) ? migInAmt : migOutAmt;
        if (transfer > 0) {
            strategyAttr->nrMigratePages[migOutNum[migOutIdx].numaId][migInNum[migInIdx].numaId] = transfer;
        }

        migInNum[migInIdx].amount -= transfer;
        migOutNum[migOutIdx].amount += transfer;
        if (migInNum[migInIdx].amount == 0) {
            migInIdx++;
        }
        if (migOutNum[migInIdx].amount == 0) {
            migOutIdx++;
        }
    }
}

static void NumaSwapMemPool(ProcessAttr *current)
{
    if (IsMultiNumaVm(current)) {
        return;
    }
    for (int i = 0; i < LOCAL_NUMA_NUM; i++) {
        for (int j = 0; j < REMOTE_NUMA_NUM; j++) {
            int l2Node = GetNrLocalNuma() + j;
            int32_t tmpNum = IsHugeMode() ? KBTo2M(current->strategyAttr.memSize[i][j]) :
                                            KBTo4K(current->strategyAttr.memSize[i][j]);
            int32_t migNum = current->strategyAttr.allocRemoteNrPages[i][j] - tmpNum;
            if (migNum > 0) {
                migNum = MIN(migNum, current->scanAttr.actcLen[l2Node]);
                current->strategyAttr.nrMigratePages[l2Node][i] = migNum;
            } else {
                migNum = MIN(-migNum, current->scanAttr.actcLen[i]);
                current->strategyAttr.nrMigratePages[i][l2Node] = migNum;
            }
        }
    }
}

static void NumaMigReduceDeal(ProcessAttr *current)
{
    if (GetRunMode() == MEM_POOL_MODE) {
        NumaSwapMemPool(current);
    } else {
        CalProcessNuma(&current->strategyAttr);
    }
    for (int i = 0; i < MAX_NODES; i++) {
        for (int j = 0; j < MAX_NODES; j++) {
            if (current->strategyAttr.nrMigratePages[i][j] > 0) {
                SMAP_LOGGER_INFO("After reduce migrate info: from: %d, to %d, num: %u.", i, j,
                                 current->strategyAttr.nrMigratePages[i][j]);
            }
        }
    }
}

static int PreMigration(struct ProcessManager *manager, struct MigrateMsg *mMsg, uint64_t *migratePages)
{
    int ret;
    ProcessAttr *current;
    bool isForcedSingleThread = false;

    EnvMutexLock(&manager->lock);
    ret = InitMigrateMsg(mMsg, manager);
    if (ret) {
        EnvMutexUnlock(&manager->lock);
        SMAP_LOGGER_ERROR("InitMigrateMsg failed! ret:%d.", ret);
        return ret;
    }
    for (current = manager->processes; current; current = current->next) {
        if (current->scanType != NORMAL_SCAN) {
            continue;
        }
        SMAP_LOGGER_INFO("+++++++scan_and_strategy_thread: processing pid %d.", current->pid);
        NumaMigReduceDeal(current);
        if (current->state != PROC_IDLE) {
            SMAP_LOGGER_DEBUG("pid %d state is %d, skip migration.", current->pid, current->state);
            continue;
        }
        current->state = PROC_MIGRATE;
        SMAP_LOGGER_DEBUG("change pid %d state from idle to migrate.", current->pid);
        // 识别每个进程的待迁移冷热页
        ret = BuildMigrationMsg(current, mMsg, migratePages);
        SMAP_LOGGER_INFO("Add process: %d to migrate msg ret: %d.", current->pid, ret);
        isForcedSingleThread = isForcedSingleThread || current->vmPidAttr.mmapType;
    }
    EnvMutexUnlock(&manager->lock);

    SetMigrateThreadNum(mMsg, *migratePages, isForcedSingleThread);
    return 0;
}

static void PostMigration(struct ProcessManager *manager, struct MigrateMsg *mMsg)
{
    EnvMutexLock(&manager->lock);
    UpdateMigResult(mMsg, manager);
    free(mMsg->migList);
    mMsg->migList = NULL;
    for (ProcessAttr *current = manager->processes; current; current = current->next) {
        if (current->state == PROC_MIGRATE) {
            SMAP_LOGGER_DEBUG("set pid %d state from migrate to idle.", current->pid);
            current->state = PROC_IDLE;
        }
    }
    EnvMutexUnlock(&manager->lock);
}

static int PerformMigration(struct ProcessManager *manager)
{
    int ret;
    uint64_t migratePages = 0;
    struct MigrateMsg mMsg;
    struct timeval start, end;

    ret = PreMigration(manager, &mMsg, &migratePages);
    if (ret) {
        SMAP_LOGGER_ERROR("PreMigration failed! ret: %d.", ret);
        return ret;
    }

    // 调用迁移接口
    gettimeofday(&start, NULL);
    ret = DoMigration(&mMsg, manager);
    gettimeofday(&end, NULL);
    PrintMigSpeed(manager, migratePages, start, end);
    SMAP_LOGGER_INFO("Do migration result: %d.", ret);
    PostMigration(manager, &mMsg);
    if (ret) {
        SMAP_LOGGER_INFO("Do migration failed! migration_failure_count=%d.", ret);
        return ret;
    }
    return ret;
}

static int UpdateScanTime(ProcessAttr *process)
{
    struct AccessAddPidPayload payload;
    payload.pid = process->pid;
    payload.numaNodes = process->numaAttr.numaNodes;
    payload.type = process->scanType;
    payload.duration = process->duration;
    if (GetFileConfSwitchConfig()) {
        payload.scanTime = GetScanPeriodConfig();
    } else {
        payload.scanTime = process->sceneInfo.cycles.scanCycle;
    }
    return AccessIoctlAddPid(1, &payload);
}

static void UpdateScene(struct ProcessManager *manager)
{
    EnvMutexLock(&manager->lock);
    ProcessAttr *current = manager->processes;
    while (current) {
        if (current->type != VM_TYPE) {
            current = current->next;
            continue;
        }
        // 更新虚机的场景
        if (current->scanType == NORMAL_SCAN) {
            SetProcessSceneAttr(current);
        }
        current = current->next;
    }
    EnvMutexUnlock(&manager->lock);
}

static int HandleScene(ThreadCtx *ctx)
{
    int ret = 0;
    Scene worstScene = LIGHT_STABLE_SCENE, scene;
    struct ProcessManager *manager = ctx->processManager;
    EnvMutexLock(&manager->lock);
    ProcessAttr *current = manager->processes;
    while (current) {
        // 更新虚机的场景
        SceneInfo *info = &current->sceneInfo;
        GetProcessSceneAttr(info->currScene, info);
        if (info->currScene != info->lastScene) {
            ret = UpdateScanTime(current);
            SMAP_LOGGER_INFO("Update pid %d scan cycle to %dms.", current->pid, current->sceneInfo.cycles.scanCycle);
            if (ret) {
                SMAP_LOGGER_ERROR("Update scan time failed for pid %d.", current->pid);
            }
        }
        scene = current->sceneInfo.currScene;
        if (scene > worstScene) {
            worstScene = scene;
        }
        current = current->next;
    }
    EnvMutexUnlock(&manager->lock);

    if (manager->sceneInfo.currScene != worstScene) {
        SMAP_LOGGER_INFO("Manager changed scene from %d to %d.", manager->sceneInfo.currScene, worstScene);
        manager->sceneInfo.currScene = worstScene;
        GetProcessSceneAttr(worstScene, &manager->sceneInfo);
        SceneCycle *cycles = &manager->sceneInfo.cycles;
        ctx->period = cycles->migCycle;
    }
    return ret;
}

static void UpdateAllProcessScanTime(ThreadCtx *ctx)
{
    int ret;
    struct ProcessManager *manager = ctx->processManager;
    ProcessAttr *current;
    EnvMutexLock(&manager->lock);
    current = manager->processes;
    while (current) {
        // 更新虚机的场景
        if (current->scanType == NORMAL_SCAN) {
            ret = UpdateScanTime(current);
            if (ret) {
                SMAP_LOGGER_WARNING("Update pid %d scan cycle failed, ret=%d.", current->pid, ret);
            }
            SMAP_LOGGER_INFO("Update pid %d scan cycle to %u.", current->pid, GetScanPeriodConfig());
        }
        current = current->next;
    }
    EnvMutexUnlock(&manager->lock);
}

static void UpdatePeriodFromConfig(ThreadCtx *ctx)
{
    if (!GetFileConfSwitchConfig()) {
        return;
    }

    if (GetMigratePeriodChanged()) {
        SMAP_LOGGER_INFO("Start update migrate period time from config to %u.", GetMigratePeriodConfig());
        ctx->period = GetMigratePeriodConfig();
        SetMigratePeriodChanged(false);
    }

    // 为了计算ntimes需要先更新迁移周期
    if (GetScanPeriodChanged()) {
        SMAP_LOGGER_INFO("Start update scan period time from config\n");
        UpdateAllProcessScanTime(ctx);
        SetScanPeriodChanged(false);
    }
}

// 管理线程函数
int ScanMigrateWork(ThreadCtx *ctx)
{
    int ret = 0;
    struct ProcessManager *manager = ctx->processManager;
    ProcessAttr *current;

    ret = DisableTracking(manager);
    if (ret) {
        SMAP_LOGGER_ERROR("Disable tracking failed! ret:%d.", ret);
        goto out;
    }
    SMAP_LOGGER_DEBUG("Tracking disabled.");
    // 由于进程销毁是异步，后续涉及ProcessAttr需要合理处理异常
    CheckAndRemoveInvalidProcess();
    ret = PerformMigrationPreparation(manager);
    if (ret) {
        SMAP_LOGGER_DEBUG("Migration preparation failed: %d.", ret);
        goto out;
    }
    // 更新虚机所处的场景
    UpdateScene(manager);
    SMAP_LOGGER_DEBUG("Scene updated.");
    // 根据内存使用情况更新配比
    ConfigRatios(manager);
    SMAP_LOGGER_DEBUG("Ratio configured.");
    // 处理迁移参数
    PeriodConfigRead(PERIOD_CONFIG_PATH); // 从配置文件中读取周期配置
    if (GetFileConfSwitchConfig()) {
        SMAP_LOGGER_DEBUG("Updating period from config.");
        UpdatePeriodFromConfig(ctx);
    } else {
        ret = HandleScene(ctx);
        if (ret) {
            SMAP_LOGGER_ERROR("Handle scene failed! ret:%d.", ret);
            goto out;
        }
        SMAP_LOGGER_DEBUG("Handle scene done.");
    }
    ret = PerformMigration(manager);
    SMAP_LOGGER_INFO("Migration result: %d.", ret);
out:
    // 启动扫描
    EnableTracking(manager);
    SMAP_LOGGER_DEBUG("Tracking enabled.");
    return ret;
}

int MigrateRemoteNuma(struct ProcessManager *manager, struct MigrateNumaIoctlMsg *msg)
{
    SMAP_LOGGER_INFO("src %d, dest %d, addr count %d\n", msg->srcNid, msg->destNid, msg->count);
    for (int i = 0; i < msg->count; i++) {
        SMAP_LOGGER_INFO("memid[%d] %llu\n", i, msg->memids[i]);
    }

    int ret = ioctl(manager->fds.migrate, SMAP_MIG_MIGRATE_NUMA, msg);
    SMAP_LOGGER_INFO("migrate numa ioctl ret: %d.", ret);
    return ret ? -ENOMEM : 0;
}
