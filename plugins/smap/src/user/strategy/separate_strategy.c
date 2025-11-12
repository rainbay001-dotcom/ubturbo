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
#include <errno.h>
#include <sys/param.h>
#include <math.h>
#include "securec.h"
#include "smap_user_log.h"
#include "manage/manage.h"
#include "strategy.h"
#include "period_config.h"
#include "separate_strategy.h"

#define DEFAULT_FREQ_WT_2M 1
#define DEFAULT_SLOW_THRED_2M (2 * DEFAULT_FREQ_WT_2M)
#define MAX_MIGRATE_DIVISOR_2M 1

static int InitSeparateParam(ProcessAttr *process)
{
    process->separateParam.freqWt = DEFAULT_FREQ_WT_2M;
    process->separateParam.slowThred = GetSlowThresholdConfig();
    SMAP_LOGGER_DEBUG("InitSeparateParam done, freqWt: %u, slowThred: %u.", process->separateParam.freqWt,
                      process->separateParam.slowThred);
}

static bool ShouldMigrate(ProcessAttr *process)
{
    if (process->separateParam.maxMigrate < 1) {
        SMAP_LOGGER_DEBUG("Pid %d maxMigrate is invalid, skip migrate.", process->pid);
        return false;
    }
    if (process->walkPage.nrPage < 1) {
        SMAP_LOGGER_DEBUG("Pid %d nrPage is invalid, skip migrate.", process->pid);
        return false;
    }
    return true;
}

static int ActcFreqAscFunc(const void *actc1, const void *actc2)
{
    ActcData *a1 = (ActcData *)actc1, *a2 = (ActcData *)actc2;
    if (a1->isWhiteListPage != a2->isWhiteListPage) {
        return a1->isWhiteListPage ? 1 : -1;
    }
    if (a1->freq < a2->freq) {
        return -1;
    } else if (a1->freq > a2->freq) {
        return 1;
    } else {
        return (int)a2->prior - (int)a1->prior;
    }
}

static int ActcFreqDescFunc(const void *actc1, const void *actc2)
{
    ActcData *a1 = (ActcData *)actc1, *a2 = (ActcData *)actc2;
    if (a1->isWhiteListPage != a2->isWhiteListPage) {
        return a1->isWhiteListPage ? 1 : -1;
    }
    if (a1->freq < a2->freq) {
        return 1;
    } else if (a1->freq > a2->freq) {
        return -1;
    } else {
        return (int)a1->prior - (int)a2->prior;
    }
}

static uint64_t CalLowMigrateNum(uint64_t migrateNum, uint64_t freqWt, uint32_t slowThred, ProcessAttr *process)
{
    uint64_t low = 0, high = migrateNum;
    int l1Node = GetAttrL1(process);
    int l2Node = GetAttrL2(process);
    if (l1Node < 0 || l2Node < 0) {
        SMAP_LOGGER_ERROR("CalcMigrateNumByFreq pid %d L1 %d or L2 %d is invalid.", process->pid, l1Node, l2Node);
        return 0;
    }
    // calculate migrate num based on freq
    while (low < high) {
        uint64_t mid = low + (high - low + 1) / 2;
        if (mid < 1) {
            return 0;
        }
        uint64_t freqL1 = freqWt * process->scanAttr.actcData[l1Node][mid - 1].freq;
        uint64_t freqL2 = process->scanAttr.actcData[l2Node][mid - 1].freq;
        if (((freqL1 == 0) && (freqL2 > 0)) || (freqL1 + slowThred < freqL2)) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }
    return low;
}

/*
 * Calculate migrate num by frequency
 *
 * Before call this function, make sure process->actcData[L1] is in ascending order
 * and process->actcData[L2] is in descending order
 */
static uint64_t CalcMigrateNumByFreq(ProcessAttr *process)
{
    uint64_t migrateNum, low = 0, high;
    uint64_t freqWt;
    uint64_t freqNum;
    uint32_t slowThred;
    uint16_t l1FreqMax, l2FreqMax;
    ActCount *l1Act = GetL1ActCount(process);
    ActCount *l2Act = GetL2ActCount(process);
    int l2Node = GetAttrL2(process);
    int l1Len = GetL1ActcLen(process);
    int l2Len = GetL2ActcLen(process);
    int remoteFreePages = GetNrFreeHugePagesByNode(l2Node);

    // calculate migrate num based on actc length, max migrate num and free page num
    migrateNum = MIN(l1Len, l2Len);
    migrateNum = MIN(migrateNum, process->separateParam.maxMigrate);
    migrateNum = MIN(migrateNum, remoteFreePages);
    migrateNum = MIN(migrateNum, (l2Act ? l2Act->freqNum : 0));

    // calculate freq weight
    l1FreqMax = (l1Act ? l1Act->freqMax : 0);
    l2FreqMax = (l2Act ? l2Act->freqMax : 0);
    // removing excessively large outliers
    if (l2Len > 0 && l2Act && l2Act->freqMax > 0) {
        int percentile = GetRemoteFreqPercentileConfig();
        double num_to_skip = ((double)(PERCENTAGE_BASE_INT - percentile) / PERCENTAGE_BASE_DOUBLE) * l2Len;
        size_t max_index = (size_t)floor(num_to_skip);
        l2FreqMax = process->scanAttr.actcData[l2Node][max_index].freq;
    } else {
        l2FreqMax = 0;
    }
    if (l1FreqMax) {
        freqWt = (uint64_t)((double)l2FreqMax / l1FreqMax);
    } else {
        freqWt = l2FreqMax;
    }
    freqWt = MAX(freqWt, process->separateParam.freqWt);
    freqWt = GetFreqWtConfig() == 0 ? freqWt : GetFreqWtConfig();
    slowThred = process->separateParam.slowThred * freqWt;
    SMAP_LOGGER_DEBUG("Pid %d freqWt %lu, slowThred: %u.", process->pid, freqWt, slowThred);

    migrateNum = CalLowMigrateNum(migrateNum, freqWt, slowThred, process);
    return migrateNum;
}

static void FreeMlist(struct MigList mlist[MAX_NODES][MAX_NODES])
{
    for (int from = 0; from < MAX_NODES; from++) {
        for (int to = 0; to < MAX_NODES; to++) {
            if (mlist[from][to].addr) {
                free(mlist[from][to].addr);
                mlist[from][to].addr = NULL;
            }
            mlist[from][to].nr = 0;
        }
    }
}

static int BaseStrategyInner(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], int from, int to)
{
    uint32_t nrMig = mlist[from][to].nr;
    if (nrMig > 0) {
        mlist[from][to].nr = nrMig;
        mlist[from][to].addr = calloc(nrMig, sizeof(uint64_t));
        if (!mlist[from][to].addr) {
            return -ENOMEM;
        }
        if (!process->scanAttr.actcData[from]) {
            return -EINVAL;
        }
        for (uint32_t idx = 0; idx < nrMig && idx < process->scanAttr.actcLen[from]; idx++) {
            mlist[from][to].addr[idx] = process->scanAttr.actcData[from][idx].addr;
        }
    }
    return 0;
}

static int BaseStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], uint64_t rawMigrateNum,
                        MigrateDirection dir)
{
    int ret = 0;
    NodeLevel level;
    int l1Node = GetAttrL1(process);
    int l2Node = GetAttrL2(process);

    if (dir > SWAP || dir < DEMOTE) {
        return -EINVAL;
    }
    if (l1Node < 0 || l2Node < 0) {
        SMAP_LOGGER_ERROR("BaseStrategy pid %d L1 %d or L2 %d is invalid.", process->pid, l1Node, l2Node);
        return -EINVAL;
    }

    // 1. calculate migrate num based on raw migrate num
    qsort(process->scanAttr.actcData[l1Node], GetL1ActcLen(process), sizeof(ActcData), ActcFreqAscFunc);
    qsort(process->scanAttr.actcData[l2Node], GetL2ActcLen(process), sizeof(ActcData), ActcFreqDescFunc);
    uint64_t SwapMigrateNum = CalcMigrateNumByFreq(process);
    mlist[l1Node][l2Node].nr = mlist[l2Node][l1Node].nr = SwapMigrateNum;
    if (dir == DEMOTE) {
        mlist[l1Node][l2Node].nr = MAX(SwapMigrateNum, rawMigrateNum);
        mlist[l2Node][l1Node].nr = mlist[l1Node][l2Node].nr - rawMigrateNum;
    } else if (dir == PROMOTE) {
        mlist[l2Node][l1Node].nr = MAX(SwapMigrateNum, rawMigrateNum);
        mlist[l1Node][l2Node].nr = mlist[l2Node][l1Node].nr - rawMigrateNum;
    } else {
        mlist[l1Node][l2Node].nr = mlist[l2Node][l1Node].nr = SwapMigrateNum;
    }

    for (int from = 0; from < MAX_NODES; from++) {
        for (int to = 0; to < MAX_NODES; to++) {
            ret = BaseStrategyInner(process, mlist, from, to);
            if (ret) {
                SMAP_LOGGER_ERROR("BaseStrategy pid %d from %d to %d failed %d.", process->pid, from, to, ret);
                FreeMlist(mlist);
                return ret;
            }
        }
    }
    process->nrMigratePage = rawMigrateNum;
    process->strategyAttr.dir[l1Node] = dir;
    process->strategyAttr.dir[l2Node] = dir;
    return 0;
}

static int PromotionStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], uint64_t rawMigrateNum)
{
    SMAP_LOGGER_INFO("Pid %d promotion, raw migrate num %lu.", process->pid, rawMigrateNum);
    return BaseStrategy(process, mlist, rawMigrateNum, PROMOTE);
}

static int DemotionStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], uint64_t rawMigrateNum)
{
    SMAP_LOGGER_INFO("Pid %d demotion, raw migrate num %lu.", process->pid, rawMigrateNum);
    return BaseStrategy(process, mlist, rawMigrateNum, DEMOTE);
}

static int SwapStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES])
{
    SMAP_LOGGER_INFO("Pid %d swap.", process->pid);
    return BaseStrategy(process, mlist, 0, SWAP);
}

int SeparateStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES])
{
    if (process->separateParam.freqWt == 0) {
        InitSeparateParam(process);
    }
    CalcMaxMigrate(MAX_MIGRATE_DIVISOR_2M, process->walkPage.nrPage, &process->separateParam.maxMigrate);
    SMAP_LOGGER_INFO("Pid %d maxMigrate: %llu.", process->pid, process->separateParam.maxMigrate);

    if (!ShouldMigrate(process)) {
        return 0;
    }

    int l1Node = GetAttrL1(process);
    int l2Node = GetAttrL2(process);
    SMAP_LOGGER_DEBUG("SeparateStrategy VM Pid %d l1Node: %d l2Node: %d.", process->pid, l1Node, l2Node);

    if (l1Node < 0 || l2Node < 0) {
        SMAP_LOGGER_ERROR("SeparateStrategy pid %d L1 %d or L2 %d is invalid.", process->pid, l1Node, l2Node);
        return -EINVAL;
    }

    if (process->strategyAttr.nrMigratePages[l1Node][l2Node] > 0) {
        return DemotionStrategy(process, mlist, process->strategyAttr.nrMigratePages[l1Node][l2Node]);
    } else if (process->strategyAttr.nrMigratePages[l2Node][l1Node] > 0) {
        return PromotionStrategy(process, mlist, process->strategyAttr.nrMigratePages[l2Node][l1Node]);
    }

    return SwapStrategy(process, mlist);
}

static void SortActcData(ProcessAttr *process)
{
    ScanAttribute *scan = &process->scanAttr;
    for (int n = 0; n < GetNrLocalNuma(); n++) {
        if (NotInAttrL1(process, n)) {
            continue;
        }
        qsort(scan->actcData[n], scan->actcLen[n], sizeof(ActcData), ActcFreqAscFunc);
    }
    for (int n = GetNrLocalNuma(); n < MAX_NODES; n++) {
        if (NotInAttrL2(process, n)) {
            continue;
        }
        qsort(scan->actcData[n], scan->actcLen[n], sizeof(ActcData), ActcFreqDescFunc);
    }
}

static uint64_t CalcMigrateNumByFreq4K(ProcessAttr *process, int localNid, int remoteNid, const uint64_t numaOffset[],
                                       const uint64_t numaFreePage[])
{
    uint64_t migrateNum, low = 0, high;
    uint32_t slowThred;
    uint64_t lastFreqNum;
    ActCount localActCount = process->scanAttr.actCount[localNid];
    ActCount remoteActCount = process->scanAttr.actCount[remoteNid];
    uint64_t localLen = process->scanAttr.actcLen[localNid] - numaOffset[localNid];
    uint64_t remoteLen = process->scanAttr.actcLen[remoteNid] - numaOffset[remoteNid];
    if (remoteActCount.freqNum > numaOffset[remoteNid]) {
        lastFreqNum = remoteActCount.freqNum - numaOffset[remoteNid];
    } else {
        lastFreqNum = 0;
    }
    // calculate migrate num based on actc length, max migrate num and free page num
    migrateNum = MIN(localLen, remoteLen);
    migrateNum = MIN(migrateNum, process->separateParam.maxMigrate);
    migrateNum = MIN(migrateNum, numaFreePage[localNid]);
    migrateNum = MIN(migrateNum, numaFreePage[remoteNid]);
    migrateNum = MIN(migrateNum, lastFreqNum);

    // slow threshold
    slowThred = MAX(STRATEGY_MIN_HOT_COLD_THRED_VALUE, (localActCount.freqMax / STRATEGY_FREQ_THRED_DIVISOR));
    // calculate migrate num based on freq
    high = migrateNum;
    while (low < high) {
        uint64_t mid = low + (high - low + 1) / 2;
        uint64_t freqL1 = process->scanAttr.actcData[localNid][mid - 1 + numaOffset[localNid]].freq;
        uint64_t freqL2 = process->scanAttr.actcData[remoteNid][mid - 1 + numaOffset[remoteNid]].freq;
        if (((freqL1 == 0) && (freqL2 > 0)) || (freqL1 + slowThred < freqL2)) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }
    migrateNum = low;
    return migrateNum;
}

static int BuildMlistAddr(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                          uint64_t numaOffset[MAX_NODES], int from, int to)
{
    uint32_t nrMig = mlist[from][to].nr;
    if (nrMig > 0) {
        mlist[from][to].nr = nrMig;
        mlist[from][to].addr = calloc(nrMig, sizeof(uint64_t));
        if (!mlist[from][to].addr) {
            return -ENOMEM;
        }
        if (!process->scanAttr.actcData[from]) {
            return -EINVAL;
        }
        for (int idx = 0; idx < nrMig && idx < process->scanAttr.actcLen[from] - numaOffset[from]; idx++) {
            mlist[from][to].addr[idx] = process->scanAttr.actcData[from][idx + numaOffset[from]].addr;
        }
    }
    return 0;
}

static uint64_t CalcSwapNum4K(ProcessAttr *process, int localNid, int remoteNid, const uint64_t numaOffset[],
                              const uint64_t numaFreePage[])
{
    uint64_t migrateNum;
    uint64_t lastFreqNum;
    uint64_t lastZeroNum;
    ActCount *localActCount = &process->scanAttr.actCount[localNid];
    ActCount *remoteActCount = &process->scanAttr.actCount[remoteNid];
    uint64_t localLen = process->scanAttr.actcLen[localNid] - numaOffset[localNid];
    uint64_t remoteLen = process->scanAttr.actcLen[remoteNid] - numaOffset[remoteNid];
    if (localActCount->freqZero > numaOffset[localNid]) {
        lastZeroNum = localActCount->freqZero - numaOffset[localNid];
    } else {
        lastZeroNum = 0;
    }
    if (remoteActCount->freqNum > numaOffset[remoteNid]) {
        lastFreqNum = remoteActCount->freqNum - numaOffset[remoteNid];
    } else {
        lastFreqNum = 0;
    }
    migrateNum = MIN(localLen, remoteLen);
    migrateNum = MIN(migrateNum, process->separateParam.maxMigrate);
    migrateNum = MIN(migrateNum, numaFreePage[localNid]);
    migrateNum = MIN(migrateNum, numaFreePage[remoteNid]);
    migrateNum = MIN(migrateNum, lastZeroNum);
    migrateNum = MIN(migrateNum, lastFreqNum);
    return migrateNum;
}

static void FindThreshold(const SelectionMode mode, uint64_t nrMig, const uint32_t *buckets, int *thresholdFreq,
                          uint32_t *takeAtThreshold)
{
    (*thresholdFreq) = mode == SELECT_TOP_K ? 0 : STRATEGY_ACTC_MAX_FREQ;
    (*takeAtThreshold) = 0;
    size_t countSoFar = 0;

    if (mode == SELECT_TOP_K) {
        for (int i = STRATEGY_ACTC_MAX_FREQ - 1; i >= 0; --i) {
            if (buckets[i] == 0) {
                continue;
            }
            if (countSoFar + buckets[i] >= nrMig) {
                (*thresholdFreq) = i;
                (*takeAtThreshold) = nrMig - countSoFar;
                break;
            }
            countSoFar += buckets[i];
        }
    } else {
        for (int i = 0; i < STRATEGY_ACTC_MAX_FREQ; ++i) {
            if (buckets[i] == 0) {
                continue;
            }
            if (countSoFar + buckets[i] >= nrMig) {
                (*thresholdFreq) = i;
                (*takeAtThreshold) = nrMig - countSoFar;
                break;
            }
            countSoFar += buckets[i];
        }
    }
}

static void CollectPages(const SelectionMode mode, uint64_t offset, uint64_t actcLen, ActcData *currentData,
                         struct MigList *currMlist, uint64_t nrMig, int thresholdFreq, uint32_t takeAtThreshold)
{
    uint32_t tmp = takeAtThreshold;
    size_t collected_count = 0;
    size_t write_idx = offset;
    for (size_t i = offset; i < actcLen && collected_count < nrMig; ++i) {
        int freq = currentData[i].freq;
        bool shouldTake = false;

        if (mode == SELECT_TOP_K) {
            shouldTake = (freq > thresholdFreq) || (freq == thresholdFreq && tmp > 0);
        } else {
            shouldTake = (freq < thresholdFreq) || (freq == thresholdFreq && tmp > 0);
        }

        if (shouldTake && !currentData[i].isWhiteListPage) {
            currMlist->addr[collected_count++] = currentData[i].addr;
            if (i != write_idx) {
                ActcData temp = currentData[i];
                currentData[i] = currentData[write_idx];
                currentData[write_idx] = temp;
            }
            write_idx++;
            if (freq == thresholdFreq) {
                tmp--;
            }
        }
    }
}

static int BuildSelectKMlistAddr(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                                 const uint64_t numaOffset[MAX_NODES], int from, int to, SelectionMode mode)
{
    uint64_t offset = numaOffset[from];
    uint64_t n = process->scanAttr.actcLen[from];
    ActcData *currentData = process->scanAttr.actcData[from];
    struct MigList *currentMig = &mlist[from][to];
    if (offset >= n) {
        currentMig->nr = 0;
        return 0;
    }
    uint64_t rangeLen = n - offset;
    uint64_t nrMig = mlist[from][to].nr;
    nrMig = MIN(nrMig, rangeLen);
    currentMig->nr = nrMig;
    if (nrMig == 0) {
        return 0;
    }
    if (!currentData) {
        currentMig->nr = 0;
        currentMig->addr = NULL;
        return -EINVAL;
    }
    currentMig->addr = calloc(nrMig, sizeof(uint64_t));
    if (!currentMig->addr) {
        return -ENOMEM;
    }
    uint32_t *buckets = (uint32_t *)calloc(STRATEGY_ACTC_MAX_FREQ, sizeof(uint32_t));
    if (buckets == NULL) {
        free(currentMig->addr);
        currentMig->addr = NULL;
        return -ENOMEM;
    }
    for (uint64_t i = offset; i < n; ++i) {
        if (currentData[i].isWhiteListPage) {
            continue;
        }
        int freq = currentData[i].freq;
        freq = MIN(freq, STRATEGY_ACTC_MAX_FREQ - 1);
        buckets[freq]++;
    }
    int thresholdFreq;
    uint32_t takeAtThreshold;
    FindThreshold(mode, nrMig, buckets, &thresholdFreq, &takeAtThreshold);
    CollectPages(mode, offset, n, currentData, currentMig, nrMig, thresholdFreq, takeAtThreshold);
    free(buckets);
    return 0;
}

static int SeparateStrategy4KInner(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                                   uint64_t numaOffset[MAX_NODES], uint64_t numaFreePage[MAX_NODES], int localNid,
                                   int remoteNid)
{
    int ret = 0;
    StrategyAttribute *strat = &process->strategyAttr;
    uint64_t swapNum = CalcSwapNum4K(process, localNid, remoteNid, numaOffset, numaFreePage);

    uint32_t demotePageNr = strat->nrMigratePages[localNid][remoteNid];
    uint32_t promotePageNr = strat->nrMigratePages[remoteNid][localNid];
    SMAP_LOGGER_INFO("Pid %lld NUMA%u to NUMA%u swap %llu demote %u promote %u\n", process->pid, localNid, remoteNid,
                     swapNum, demotePageNr, promotePageNr);

    if (demotePageNr > 0) {
        mlist[localNid][remoteNid].nr = MAX(swapNum, demotePageNr);
        mlist[remoteNid][localNid].nr = mlist[localNid][remoteNid].nr - demotePageNr;
    } else if (promotePageNr > 0) {
        mlist[remoteNid][localNid].nr = MAX(swapNum, promotePageNr);
        mlist[localNid][remoteNid].nr = mlist[remoteNid][localNid].nr - promotePageNr;
    } else {
        mlist[remoteNid][localNid].nr = mlist[localNid][remoteNid].nr = swapNum;
    }

    ret = BuildSelectKMlistAddr(process, mlist, numaOffset, localNid, remoteNid, SELECT_BOTTOM_K);
    if (ret) {
        SMAP_LOGGER_ERROR("Build Mlist addr error.\n");
        return ret;
    }
    ret = BuildSelectKMlistAddr(process, mlist, numaOffset, remoteNid, localNid, SELECT_TOP_K);
    if (ret) {
        SMAP_LOGGER_ERROR("Build Mlist addr error.\n");
        return ret;
    }

    numaOffset[localNid] += mlist[localNid][remoteNid].nr;
    numaOffset[remoteNid] += mlist[remoteNid][localNid].nr;

    if (numaFreePage[remoteNid] > mlist[localNid][remoteNid].nr) {
        numaFreePage[remoteNid] -= mlist[localNid][remoteNid].nr;
    } else {
        numaFreePage[remoteNid] = 0;
    }
    if (numaFreePage[localNid] > mlist[remoteNid][localNid].nr) {
        numaFreePage[localNid] -= mlist[remoteNid][localNid].nr;
    } else {
        numaFreePage[localNid] = 0;
    }

    return ret;
}

int SeparateStrategy4K(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES])
{
    int ret = 0;
    if (process->separateParam.freqWt == 0) {
        InitSeparateParam(process);
    }
    CalcMaxMigrate(MAX_MIGRATE_DIVISOR_2M, process->walkPage.nrPage, &process->separateParam.maxMigrate);
    if (!ShouldMigrate(process)) {
        return 0;
    }
    uint64_t numaOffset[MAX_NODES] = { 0 };
    uint64_t numaFreePages[MAX_NODES] = { 0 };
    for (int nid = 0; nid < MAX_NODES; ++nid) {
        if (InAttrL1(process, nid) || InAttrL2(process, nid)) {
            numaFreePages[nid] = GetNrFreePagesByNode(nid);
        }
    }

    int localNumaNum = GetNrLocalNuma();
    for (int localNid = 0; localNid < localNumaNum; localNid++) {
        if (NotInAttrL1(process, localNid)) {
            continue;
        }
        for (int remoteNid = localNumaNum; remoteNid < localNumaNum + REMOTE_NUMA_NUM; remoteNid++) {
            if (NotInAttrL2(process, remoteNid)) {
                continue;
            }
            if ((ret = SeparateStrategy4KInner(process, mlist, numaOffset, numaFreePages, localNid, remoteNid)) != 0) {
                FreeMlist(mlist);
                return ret;
            }
        }
    }
    return 0;
}
