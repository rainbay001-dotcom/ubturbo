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
#include <stdlib.h>
#include <math.h>
#include <sys/param.h>

#include "securec.h"
#include "smap_user_log.h"
#include "scene_info.h"
#include "scene.h"

static bool g_adaptLocalMem = true;

static inline int PrevIndex(int index, int len)
{
    return (index + len - 1) % len;
}

static inline int NextIndex(int index, int len)
{
    return (index + 1) % len;
}

static inline void IncPageIndex(SceneInfo *info)
{
    int next;
    next = NextIndex(info->pageInfoIndex, PAGE_INFO_DEPTH);
    info->pageInfoIndex = next;
}

static int GetDeltaHot(SceneInfo *info, int currIdx)
{
    int prevIdx;
    int deltaHot;
    PageInfo *prevPageinfo, *currPageinfo;

    prevIdx = PrevIndex(currIdx, PAGE_INFO_DEPTH);
    prevPageinfo = &info->pageInfo[prevIdx];
    currPageinfo = &info->pageInfo[currIdx];

    deltaHot = abs(currPageinfo->nrHot - prevPageinfo->nrHot);
    return deltaHot;
}

static bool TryFromStableToUnstable(SceneInfo *info, int idx)
{
    PageInfo *p = &info->pageInfo[idx];
    int deltaHot;
    deltaHot = GetDeltaHot(info, idx);
    if (p->nrHot == 0) { // 无任何热页，不认为非稳态
        return false;
    }
    if (deltaHot < p->nrPages * ENTER_UNSTABLE_THRESHOLD_RATIO ||
        deltaHot < ENTER_UNSTABLE_THRESHOLD_NUM) { // ΔHot太小则不被认定为非稳态
        return false;
    }
    if (p->nrL2Hot <= EXIT_UNSTABLE_L2HOT_THRESHOLD_NUM) { // L2几乎无访问可以不当非稳态处理
        return false;
    }

    return true;
}

static bool TryToStayInUnstable(SceneInfo *info, int idx)
{
    PageInfo *p = &info->pageInfo[idx];
    int deltaHot;
    deltaHot = GetDeltaHot(info, idx);
    if (deltaHot > p->nrPages * EXIT_UNSTABLE_THRESHOLD_RATIO &&
        deltaHot > ENTER_UNSTABLE_THRESHOLD_NUM) { // 如果持续有较小的ΔHot，仍然留在非稳态
        return true;
    }

    if (p->nrL2Hot >= EXIT_UNSTABLE_L2HOT_THRESHOLD_NUM) { // 如果L2持续有少量的访问，仍然保持为非稳态
        return true;
    }

    return false;
}

static bool IsUnstableScene(SceneInfo *info)
{
    int i;
    int currIdx;
    currIdx = info->pageInfoIndex;
    if (info->lastScene != UNSTABLE_SCENE) {
        for (i = 0; i < ENTER_UNSTABLE_WINDOW_SIZE; i++) {
            if (!TryFromStableToUnstable(info, currIdx)) {
                return false;
            }
            currIdx = PrevIndex(currIdx, PAGE_INFO_DEPTH);
        }
        return true;
    } else {
        for (i = 0; i < EXIT_UNSTABLE_WINDOW_SIZE; i++) {
            PageInfo *currPageinfo = &info->pageInfo[currIdx];
            if (currPageinfo->nrHot == 0 || currPageinfo->nrL2Hot == 0) {
                return false;
            }
            if (TryToStayInUnstable(info, currIdx)) {
                return true;
            }
            currIdx = PrevIndex(currIdx, PAGE_INFO_DEPTH);
        }
        return false;
    }
}

static bool IsHeavyLoadScene(SceneInfo *info)
{
    int i, idx;
    PageInfo *pageinfo;
    idx = info->pageInfoIndex;
    for (i = 0; i < ENTER_HEAVY_WINDOW_SIZE; i++) {
        pageinfo = &info->pageInfo[idx];
        if (pageinfo->nrL1Guarantee < pageinfo->nrPages * HEAVY_HOT_RATIO) {
            return false;
        }
        idx = PrevIndex(idx, PAGE_INFO_DEPTH);
    }

    return true;
}

static void AnalyzeScene(SceneInfo *info)
{
    info->lastScene = info->currScene;
    if (IsUnstableScene(info)) {
        info->currScene = UNSTABLE_SCENE;
    } else if (IsHeavyLoadScene(info)) {
        info->currScene = HEAVY_STABLE_SCENE;
    } else {
        info->currScene = LIGHT_STABLE_SCENE;
    }
}

static int StatsL2AvgHotPages(ProcessAttr *process)
{
    SceneInfo *info = &process->sceneInfo;
    int idx = info->pageInfoIndex;
    int avg, sum = 0;
    for (int i = 0; i < L2_CHECK_WINDOW_SIZE; i++) {
        PageInfo *p = &info->pageInfo[idx];
        sum += p->nrL2Hot;
        idx = PrevIndex(idx, PAGE_INFO_DEPTH);
    }
    avg = sum / L2_CHECK_WINDOW_SIZE;
    return avg;
}

static uint32_t StatsMaxGuaranteePages(ProcessAttr *process)
{
    SceneInfo *info = &process->sceneInfo;
    int idx = info->pageInfoIndex;
    uint32_t maxGua = 0;
    for (int i = 0; i < KEEP_GUARANTEE_WINDOW_SIZE; i++) {
        PageInfo *p = &info->pageInfo[idx];
        maxGua = MAX(maxGua, p->nrL1GuaranteeBk);
        idx = PrevIndex(idx, PAGE_INFO_DEPTH);
    }
    return maxGua;
}

static int StatsMaxHotPages(ProcessAttr *process)
{
    SceneInfo *info = &process->sceneInfo;
    int idx = info->pageInfoIndex;
    int maxHot = 0;
    for (int i = 0; i < KEEP_GUARANTEE_WINDOW_SIZE; i++) {
        PageInfo *p = &info->pageInfo[idx];
        maxHot = MAX(maxHot, p->nrHot);
        idx = PrevIndex(idx, PAGE_INFO_DEPTH);
    }
    return maxHot;
}

static void StatsGuaranteePages(ProcessAttr *process)
{
    SceneInfo *info = &process->sceneInfo;
    PageInfo *p = &info->pageInfo[info->pageInfoIndex];
    p->nrL1Guarantee = StatsMaxHotPages(process) * GUARANTEE_AFFLUENT_SIZE;
    if (p->nrL2Hot) { // 如果L2有访问，则继续增大需求
        p->nrL1Guarantee += (p->nrL2Hot * GUARANTEE_AFFLUENT_SIZE);
    }
    p->nrL1GuaranteeBk = p->nrL1Guarantee;
    uint32_t maxGuar = StatsMaxGuaranteePages(process);
    p->nrL1Guarantee = MAX(p->nrL1Guarantee, maxGuar);
    p->nrL1Guarantee = MIN(p->nrL1Guarantee, p->nrPages); // 最高为虚机规格
    p->nrL1Guarantee = MAX(p->nrL1Guarantee, p->nrPages * MIN_GUARANTEE_SIZE); // 最低保留50%
    SMAP_LOGGER_INFO("pid %d: nrGuar %u nrHot %u max %u.", process->pid, p->nrL1Guarantee, p->nrHot, maxGuar);
}

static void CalcMemInfo(ProcessAttr *process)
{
    ActCount *l1Act = GetL1ActCount(process);
    ActCount *l2Act = GetL2ActCount(process);
    SceneInfo *info = &process->sceneInfo;
    PageInfo *p = &info->pageInfo[info->pageInfoIndex];
    p->nrL1Page = l1Act ? l1Act->pageNum : 0;
    p->nrL1Hot = p->nrL1Page - (l1Act ? l1Act->freqZero : 0);

    p->nrL2Page = l2Act ? l2Act->pageNum : 0;
    p->nrL2Hot = p->nrL2Page - (l2Act ? l2Act->freqZero : 0);

    p->nrHot = p->nrL1Hot + p->nrL2Hot;
    p->nrPages = p->nrL1Page + p->nrL2Page;

    SMAP_LOGGER_INFO("L1 nrPage %u, nrHot %u.", p->nrL1Page, p->nrL1Hot);
    SMAP_LOGGER_INFO("L2 nrPage %u, nrHot %u.", p->nrL2Page, p->nrL2Hot);
}

int GetProcessSceneAttr(Scene scene, SceneInfo *info)
{
    if (scene >= SCENE_MAX || !info) {
        return -EINVAL;
    }

    if (scene == UNSTABLE_SCENE) {
        info->cycles.scanCycle = UNSTABLE_SCAN_CYCLE;
        info->cycles.migCycle = UNSTABLE_MIGRATE_CYCLE;
    } else if (scene == HEAVY_STABLE_SCENE) {
        info->cycles.scanCycle = HEAVY_STABLE_SCAN_CYCLE;
        info->cycles.migCycle = HEAVY_STABLE_MIGRATE_CYCLE;
    } else {
        info->cycles.scanCycle = LIGHT_STABLE_SCAN_CYCLE;
        info->cycles.migCycle = LIGHT_STABLE_MIGRATE_CYCLE;
    }

    return 0;
}

int InitSceneInfo(SceneInfo *info)
{
    if (!info) {
        return -EINVAL;
    }
    info->currScene = info->lastScene = LIGHT_STABLE_SCENE;
    GetProcessSceneAttr(info->currScene, info);
    info->pageInfoIndex = 0;

    return 0;
}

int SetProcessSceneAttr(ProcessAttr *process)
{
    if (!process) {
        return -EINVAL;
    }
    SceneInfo *info = &process->sceneInfo;
    IncPageIndex(info);
    CalcMemInfo(process);
    StatsGuaranteePages(process);
    AnalyzeScene(info);
    SMAP_LOGGER_INFO("Set pid %d current scene to %d.", process->pid, info->currScene);
    return 0;
}

static void SetLocalMemStatus(SceneInfo *info)
{
    PageInfo *p = &info->pageInfo[info->pageInfoIndex];
    if (p->nrL1Planed == p->nrPages) {
        info->status = FULL_SATISFIED;
    } else if (p->nrL1Planed > p->nrL1Guarantee) {
        info->status = SATISFIED;
    } else if (p->nrL1Planed == p->nrL1Guarantee) {
        info->status = STAY;
    } else {
        info->status = LESS;
    }
}

static inline int AddList(int arr[], int len)
{
    int i = 0;
    int arrSum = 0;
    for (i = 0; i < len; i++) {
        arrSum += arr[i];
    }
    return arrSum;
}

static void UpdateMemRatio(ProcessAttr *process)
{
    int l1Node = GetAttrL1(process);
    int l2Node = GetAttrL2(process);
    int nrLocalNuma = GetNrLocalNuma();
    SceneInfo *info = &process->sceneInfo;
    PageInfo *p = &info->pageInfo[info->pageInfoIndex];

    if (l1Node < 0 || l2Node < nrLocalNuma) {
        return;
    }

    process->strategyAttr.l3RemoteMemRatio[l1Node][l2Node - nrLocalNuma] =
        HUNDRED - (double)p->nrL1Planed * HUNDRED / (p->nrPages + LITTLE_NUM);
    SMAP_LOGGER_INFO("Adjust l3RemoteMemRatio to %.2lf for pid %d who need %u pages.",
                     process->strategyAttr.l3RemoteMemRatio[l1Node][l2Node - nrLocalNuma], process->pid,
                     p->nrL1Guarantee);
}

static inline void ClearList(int arr[], int len)
{
    for (int i = 0; i < len; i++) {
        arr[i] = 0;
    }
}

static void DistributeExtraPages(int arr[], int len)
{
    int i, diff;
    double posSum = 0, negSum = 0, sum = 0;
    int offered[len];

    for (i = 0; i < len; i++) {
        sum += arr[i];
        if (arr[i] > 0) {
            posSum += arr[i];
        } else {
            negSum += -arr[i];
        }
    }

    for (i = 0; i < len; i++) {
        if (arr[i] > 0) {
            diff = ceil((arr[i] / (posSum + LITTLE_NUM)) * negSum); // 过量消费，然后调整误差
            offered[i] = diff;
            arr[i] -= diff;
        } else {
            offered[i] = 0;
            arr[i] = 0;
        }
    }
    diff = sum - AddList(arr, len); // 预期为非负数
    i = 0;
    while (diff > 0) {
        if (offered[i] > 0) {
            int c = MIN(diff, offered[i]);
            arr[i] += c;
            diff -= c;
        }
        i++;
    }
}

static void DistributeInsufficientPages(int arr[], int len)
{
    int i, diff;
    double posSum = 0, negSum = 0, sum = 0;
    int consumer[len];

    for (i = 0; i < len; i++) {
        sum += arr[i];
        if (arr[i] > 0) {
            posSum += arr[i];
        } else {
            negSum += -arr[i];
        }
    }

    for (i = 0; i < len; i++) {
        if (arr[i] >= 0) {
            consumer[i] = 0;
            arr[i] = 0;
        } else if (arr[i] < 0) { // 向上取整消费
            diff = ceil((-arr[i] / (negSum + LITTLE_NUM)) * posSum);
            consumer[i] = diff;
            arr[i] += diff;
        }
    }
    diff = AddList(arr, len) - sum; // 预期为非负数
    i = 0;
    while (diff > 0) {
        if (consumer[i] > 0) {
            int c = MIN(diff, consumer[i]);
            arr[i] -= c;
            diff -= c;
        }
        i++;
    }
}

static void BalanceSurpluses(int arr[], int len)
{
    int sum = AddList(arr, len);
    if (sum == 0) {
        ClearList(arr, len);
    } else if (sum > 0) { // 如果总页数有盈余，直接满足所有有需求的虚机
        DistributeExtraPages(arr, len);
    } else { // 如果总页数有盈余，尽力将有盈余的虚机的页分配给有需求的虚机
        DistributeInsufficientPages(arr, len);
    }
}

void SetAdaptMem(bool flag)
{
    g_adaptLocalMem = flag;
}

static bool IsReadyForAdapt(ProcessAttr *attr)
{
    if (!attr->adaptMem.enableAdaptMem) {
        return false;
    }
    if (!g_adaptLocalMem) {
        return false;
    }
    if (attr->type != VM_TYPE) {
        return false;
    }

    return true;
}

static void AdjustVmMemRatio(struct ProcessManager *manager, int *surpluses, int len)
{
    int i = 0;
    int nrVms = manager->nr[VM_TYPE];
    BalanceSurpluses(surpluses, nrVms);
    ProcessAttr *current = manager->processes;
    if (len == 0) {
        return;
    }
    while (current) {
        if (IsReadyForAdapt(current)) {
            SceneInfo *info = &current->sceneInfo;
            PageInfo *p = &info->pageInfo[info->pageInfoIndex];
            p->nrL1Planed = p->nrL1Guarantee + surpluses[i];
            SMAP_LOGGER_INFO("AdjustVmMemRatio nrL1Planed: %u, nrL1Guarantee: %u, surpluses: %d.", p->nrL1Planed,
                             p->nrL1Guarantee, surpluses[i]);
            SetLocalMemStatus(info);
            if (info->status == FULL_SATISFIED) {
                info->currScene = LIGHT_STABLE_SCENE;
            } else if (info->status == SATISFIED) {
                info->currScene = MAX(LIGHT_STABLE_SCENE, (int)info->currScene - 1);
            }
            UpdateMemRatio(current);
        }
        i++;
        current = current->next;
        if (i >= len) {
            break;
        }
    }
}

void ConfigMultiVmRatio(struct ProcessManager *manager)
{
    int nrVms = manager->nr[VM_TYPE];
    if (!nrVms) {
        SMAP_LOGGER_DEBUG("No vm is found, skip adapt mem ratio.");
        return;
    }
    int i = 0, surpluses[nrVms];
    ProcessAttr *current = manager->processes;
    while (current) {
        SceneInfo *info = &current->sceneInfo;
        PageInfo *p = &info->pageInfo[info->pageInfoIndex];
        int l1Node = GetAttrL1(current);
        int l2Node = GetAttrL2(current) - manager->nrLocalNuma;
        if (l1Node >= 0 && l2Node >= 0 && IsReadyForAdapt(current)) {
            int localPages = p->nrPages * (HUNDRED - current->strategyAttr.l2RemoteMemRatio[l1Node][l2Node]) / HUNDRED;
            surpluses[i] = localPages - p->nrL1Guarantee;
        } else {
            surpluses[i] = 0; // 后续流程中不会更改配比
        }
        i++;
        current = current->next;
    }
    AdjustVmMemRatio(manager, surpluses, nrVms);
}

static void GetMaxNuma(struct ProcessManager *manager, int *maxL1node, int *maxL2node)
{
    int l1Node = 0, l2Node = 0;
    ProcessAttr *current = manager->processes;
    while (current) { // 获取本次/远端NUMA的范围
        int tmpL1Node = GetAttrL1(current);
        int tmpL2Node = GetAttrL2(current);
        if (l1Node < tmpL1Node) {
            l1Node = tmpL1Node;
        }
        if (l2Node < tmpL2Node) {
            l2Node = tmpL2Node;
        }
        current->adaptMem.enableAdaptMem = false;
        current = current->next;
    }
    *maxL1node = l1Node;
    *maxL2node = l2Node;
}

static void ConfigMultiVmRatioInGroups(struct ProcessManager *manager)
{
    int i, maxL1node = 0, maxL2node = 0;
    EnvMutexLock(&manager->lock);
    GetMaxNuma(manager, &maxL1node, &maxL2node);
    int processed[(maxL1node + 1)][(maxL2node + 1)];
    ClearList((int *)processed, (maxL1node + 1) * (maxL2node + 1));
    ProcessAttr *current = manager->processes;
    while (current) {
        if (current->scanType != NORMAL_SCAN) {
            current = current->next;
            continue;
        }

        if (IsMultiNumaVm(current)) {
            int ret = memcpy_s(current->strategyAttr.l3RemoteMemRatio,
                sizeof(double) * LOCAL_NUMA_NUM * REMOTE_NUMA_NUM,
                current->strategyAttr.l2RemoteMemRatio, sizeof(double) * LOCAL_NUMA_NUM * REMOTE_NUMA_NUM);
            SMAP_LOGGER_DEBUG("memcpy l3 remote mem ratio ret %d.", ret);
            current = current->next;
            continue;
        }

        int l1 = GetAttrL1(current);
        int l2 = GetAttrL2(current);
        if (processed[l1][l2]) { // 如果这个近端-远端组处理过了则跳过
            current = current->next;
            continue;
        }
        int nrObjects = 0;
        processed[l1][l2] = true;
        ProcessAttr *tmp = manager->processes;
        while (tmp) {
            if (tmp->scanType != NORMAL_SCAN) {
                tmp = tmp->next;
                continue;
            }
            if (EqualToAttrL1(tmp, l1) && EqualToAttrL2(tmp, l2)) {
                tmp->adaptMem.enableAdaptMem = true; // 本地/远端NUMA一致的，标记为同一个group
                nrObjects++;
            }
            tmp = tmp->next;
        }
        if (nrObjects > 0) {
            ConfigMultiVmRatio(manager); // 进行自适应配比调度
        }
        tmp = manager->processes;
        while (tmp) {
            tmp->adaptMem.enableAdaptMem = false; // 本地/远端NUMA一致的，标记为同一个group
            tmp = tmp->next;
        }
        current = current->next;
    }
    EnvMutexUnlock(&manager->lock);
}

static void ConfigMultiProcessRatio(struct ProcessManager *manager)
{
    EnvMutexLock(&manager->lock);
    ProcessAttr *current = manager->processes;
    while (current) {
        if (current->scanType != NORMAL_SCAN) {
            current = current->next;
            continue;
        }
        int ret = memcpy_s(current->strategyAttr.l3RemoteMemRatio, sizeof(double) * LOCAL_NUMA_NUM * REMOTE_NUMA_NUM,
                           current->strategyAttr.l2RemoteMemRatio, sizeof(double) * LOCAL_NUMA_NUM * REMOTE_NUMA_NUM);
        if (ret) {
            SMAP_LOGGER_ERROR("memcpy l3 remote mem ratio failed.");
        }
        current = current->next;
    }
    EnvMutexUnlock(&manager->lock);
}

void ConfigRatios(struct ProcessManager *manager)
{
    if (GetPidType(manager) == VM_TYPE) {
        ConfigMultiVmRatioInGroups(manager);
    } else {
        ConfigMultiProcessRatio(manager);
    }
}

bool GetAdaptMem(void)
{
    return g_adaptLocalMem;
}
