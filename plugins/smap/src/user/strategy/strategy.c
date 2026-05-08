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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "securec.h"
#include "smap_user_log.h"
#include "manage/manage.h"
#include "manage/swap_account.h"
#include "separate_strategy.h"
#include "strategy.h"

uint64_t GetNrFreePagesByNode(int nid)
{
    int ret;
    uint64_t nr = 0;
    FILE *file;
    char filename[BUFFER_SIZE];
    char line[BUFFER_SIZE] = { 0 };
    char fmt[BUFFER_SIZE] = "/sys/devices/system/node/node%d/meminfo";
    char seps[] = " ";
    char *token = NULL;
    int tmpCnt = FREE_BYTES_INDEX;

    ret = snprintf_s(filename, BUFFER_SIZE, BUFFER_SIZE, fmt, nid);
    if (ret == -1) {
        SMAP_LOGGER_ERROR("snprintf_s failed.");
        return 0;
    }
    file = fopen(filename, "r");
    if (!file) {
        SMAP_LOGGER_ERROR("fopen %s failed, errno: %d.", filename, errno);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, "MemFree") == NULL) {
            continue;
        }
        token = strtok(line, seps);
        while (token && tmpCnt--) {
            token = strtok(NULL, seps);
        }
        if (sscanf_s(token, "%lu", &nr) <= 0) {
            SMAP_LOGGER_ERROR("sscanf_s failed.");
            nr = 0;
        }
        break;
    }
    nr >>= KB_TO_PAGE_SHIFT;
    nr -= (nr > NR_RESERVED_PAGES ? NR_RESERVED_PAGES : nr);

    ret = fclose(file);
    if (ret) {
        SMAP_LOGGER_ERROR("close %s failed, errno: %d.", filename, errno);
    }
    return nr;
}

uint64_t GetNrFreeHugePagesByNode(int nid)
{
    int ret;
    uint64_t nr = 0;
    FILE *file;
    char filename[BUFFER_SIZE];
    char line[BUFFER_SIZE];
    char fmt[BUFFER_SIZE] = "/sys/devices/system/node/node%d/hugepages/hugepages-2048kB/free_hugepages";

    ret = snprintf_s(filename, BUFFER_SIZE, BUFFER_SIZE, fmt, nid);
    if (ret == -1) {
        SMAP_LOGGER_ERROR("snprintf_s failed.");
        return 0;
    }
    file = fopen(filename, "r");
    if (!file) {
        SMAP_LOGGER_ERROR("fopen %s failed, errno: %d.", filename, errno);
        return 0;
    }

    if (fgets(line, sizeof(line), file)) {
        if (sscanf_s(line, "%llu", &nr) <= 0) {
            SMAP_LOGGER_ERROR("sscanf_s failed.");
            nr = 0;
        }
    }

    nr -= (nr > NR_RESERVED_HUGE_PAGES ? NR_RESERVED_HUGE_PAGES : nr);

    ret = fclose(file);
    if (ret) {
        SMAP_LOGGER_ERROR("close %s failed, errno: %d.", filename, errno);
    }
    return nr;
}

int RunStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], size_t mlistSize)
{
    if (mlistSize < MAX_NODES) {
        SMAP_LOGGER_ERROR("Miglist size is small, size:%zu.", mlistSize);
        return -EINVAL;
    }
    if (!process || CheckActcDataValid(process)) {
        SMAP_LOGGER_ERROR("Invalid pid %d actc.", process ? process->pid : -1);
        return -EINVAL;
    }
    if (IsHugeMode()) {
        if (IsMultiNumaVm(process)) {
            return SeparateStrategyMultiNumaVm(process, mlist);
        } else {
            return SeparateStrategy(process, mlist);
        }
    } else {
        return SeparateStrategy4K(process, mlist);
    }
}

/* LevelActcData comparison: ascending freq (coldest first) */
static int CompLevelActcFreqAsc(const void *a, const void *b)
{
    const LevelActcData *la = (const LevelActcData *)a;
    const LevelActcData *lb = (const LevelActcData *)b;
    if (la->freq < lb->freq)
        return -1;
    if (la->freq > lb->freq)
        return 1;
    return 0;
}

static int AppendMigEntry(struct MigList mlist[MAX_NODES][MAX_NODES],
                          int from, int to, pid_t pid, uint64_t addr)
{
    struct MigList *ml = &mlist[from][to];
    if (ml->addr == NULL) {
        /* Pre-allocate conservatively; caller knows upper bound */
        ml->pid = pid;
        ml->from = from;
        ml->to = to;
        ml->nr = 0;
        /* addr array will be set up by caller before first call */
    }
    ml->addr[ml->nr++] = addr;
    return 0;
}

/*
 * BuildTieredMsg - unified three-tier page placement for nvmeRatio > 0.
 *
 * Algorithm:
 *   1. Read nvme_pages from /proc/<pid>/status VmSwap.
 *   2. Combine L1+L2 actcData into a flat array sorted by freq ascending.
 *   3. Compute target_nvme, target_l2, target_l1 from ratios.
 *   4. If nvme_pages < target_nvme: build swap list from coldest pages.
 *   5. Build L1<->L2 migrate lists for remaining pages, constrained by free pages.
 *
 * @mlist:    output L1<->L2 migration list (same format as RunStrategy output)
 * @swapList: output swap-out list indexed by source NUMA node
 */
int BuildTieredMsg(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES],
                   struct MigList swapList[MAX_NODES])
{
    int l1Node = GetAttrL1(process);
    int l2Node = GetAttrL2(process);
    if (l1Node < 0 || l2Node < 0) {
        SMAP_LOGGER_ERROR("BuildTieredMsg pid %d invalid L1=%d L2=%d.", process->pid, l1Node, l2Node);
        return -EINVAL;
    }

    uint64_t l1Len = process->scanAttr.actcLen[l1Node];
    uint64_t l2Len = process->scanAttr.actcLen[l2Node];
    uint64_t total_l1l2 = l1Len + l2Len;

    /* Convert VmSwap KB to page count */
    uint64_t vm_swap_kb = ReadVmSwap(process->pid);
    uint32_t page_size_kb = IsHugeMode() ? (GetHugePageSize() / KIB) : (GetNormalPageSize() / KIB);
    uint64_t nvme_pages = (page_size_kb > 0) ? (vm_swap_kb / page_size_kb) : 0;

    uint64_t total_pages = total_l1l2 + nvme_pages;
    if (total_pages == 0) {
        return 0;
    }

    uint64_t target_nvme = total_pages * (uint64_t)process->nvmeRatio / 100;
    int l2Index = l2Node - GetNrLocalNuma();
    double l2Ratio = (l2Index >= 0 && l2Index < REMOTE_NUMA_NUM) ?
                     process->strategyAttr.initRemoteMemRatio[l1Node][l2Index] : 0;
    uint64_t target_l2 = (uint64_t)(total_pages * l2Ratio / 100.0);

    SMAP_LOGGER_INFO("Pid %d tiered: total=%llu nvme_cur=%llu target_nvme=%llu target_l2=%llu.",
                     process->pid, total_pages, nvme_pages, target_nvme, target_l2);

    /* Build merged sorted array from L1 + L2 actcData */
    LevelActcData *merged = malloc(total_l1l2 * sizeof(LevelActcData));
    if (!merged) {
        return -ENOMEM;
    }

    uint64_t idx = 0;
    ActcData *l1Data = process->scanAttr.actcData[l1Node];
    ActcData *l2Data = process->scanAttr.actcData[l2Node];
    for (uint64_t i = 0; i < l1Len; i++) {
        merged[idx].addr = l1Data[i].addr;
        merged[idx].freq = l1Data[i].freq;
        merged[idx].node = l1Node;
        idx++;
    }
    for (uint64_t i = 0; i < l2Len; i++) {
        merged[idx].addr = l2Data[i].addr;
        merged[idx].freq = l2Data[i].freq;
        merged[idx].node = l2Node;
        idx++;
    }
    qsort(merged, total_l1l2, sizeof(LevelActcData), CompLevelActcFreqAsc);

    /* Pre-allocate addr arrays for swapList and mlist */
    for (int n = 0; n < MAX_NODES; n++) {
        if (total_l1l2 > 0) {
            swapList[n].addr = calloc(total_l1l2, sizeof(uint64_t));
            if (!swapList[n].addr) {
                for (int m = 0; m < n; m++) {
                    free(swapList[m].addr);
                    swapList[m].addr = NULL;
                }
                free(merged);
                return -ENOMEM;
            }
            swapList[n].nr = 0;
            swapList[n].pid = process->pid;
            swapList[n].from = n;
            swapList[n].to = -1;
        }
    }

    uint64_t delta_nvme = (nvme_pages < target_nvme) ? (target_nvme - nvme_pages) : 0;
    uint64_t swap_count = MIN(delta_nvme, total_l1l2);

    /* Get free page headroom for promote (L2->L1) */
    uint64_t free_l1 = IsHugeMode() ? GetNrFreeHugePagesByNode(l1Node) : GetNrFreePagesByNode(l1Node);
    /* Add pages that will be freed by swap-out as available headroom */
    uint64_t promote_budget = free_l1 + swap_count;

    uint64_t demote_addr_buf_len = l1Len + 1;
    uint64_t promote_addr_buf_len = l2Len + 1;
    if (demote_addr_buf_len > 0) {
        mlist[l1Node][l2Node].addr = calloc(demote_addr_buf_len, sizeof(uint64_t));
        mlist[l1Node][l2Node].nr = 0;
        mlist[l1Node][l2Node].pid = process->pid;
        mlist[l1Node][l2Node].from = l1Node;
        mlist[l1Node][l2Node].to = l2Node;
    }
    if (promote_addr_buf_len > 0) {
        mlist[l2Node][l1Node].addr = calloc(promote_addr_buf_len, sizeof(uint64_t));
        mlist[l2Node][l1Node].nr = 0;
        mlist[l2Node][l1Node].pid = process->pid;
        mlist[l2Node][l1Node].from = l2Node;
        mlist[l2Node][l1Node].to = l1Node;
    }

    if ((!mlist[l1Node][l2Node].addr && demote_addr_buf_len > 0) ||
        (!mlist[l2Node][l1Node].addr && promote_addr_buf_len > 0)) {
        for (int n = 0; n < MAX_NODES; n++) {
            free(swapList[n].addr);
            swapList[n].addr = NULL;
        }
        free(mlist[l1Node][l2Node].addr);
        mlist[l1Node][l2Node].addr = NULL;
        free(mlist[l2Node][l1Node].addr);
        mlist[l2Node][l1Node].addr = NULL;
        free(merged);
        return -ENOMEM;
    }

    uint64_t promote_used = 0;

    for (uint64_t i = 0; i < total_l1l2; i++) {
        int nid = merged[i].node;
        uint64_t addr = merged[i].addr;

        if (i < swap_count) {
            /* Coldest pages → NVMe swap */
            swapList[nid].addr[swapList[nid].nr++] = addr;
        } else if (i < swap_count + target_l2) {
            /* Next tier → L2 */
            if (nid == l1Node) {
                /* L1 → L2 demote */
                mlist[l1Node][l2Node].addr[mlist[l1Node][l2Node].nr++] = addr;
            }
            /* Already in L2: no action needed */
        } else {
            /* Hottest tier → L1 */
            if (nid == l2Node && promote_used < promote_budget) {
                /* L2 → L1 promote */
                mlist[l2Node][l1Node].addr[mlist[l2Node][l1Node].nr++] = addr;
                promote_used++;
            }
            /* Already in L1: no action needed */
        }
    }

    SMAP_LOGGER_INFO("Pid %d tiered result: swap=%llu demote=%llu promote=%llu.",
                     process->pid, swap_count, mlist[l1Node][l2Node].nr, mlist[l2Node][l1Node].nr);

    free(merged);
    return 0;
}
