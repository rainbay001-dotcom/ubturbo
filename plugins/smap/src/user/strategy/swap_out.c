/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "smap_user_log.h"
#include "securec.h"
#include "manage/manage.h"
#include "manage/smap_ioctl.h"
#include "cold_tracker.h"
#include "swap_out.h"

/*
 * DoSwapOut - Swap cold pages from L1/L2 to NVMe via SMAP_MIG_SWAP_OUT ioctl.
 *
 * Strictly enforces max_pages per cycle across tracked NUMA nodes, resets
 * cold_windows counters for submitted pages, and preserves shadow pointers
 * across the kernel ioctl (which overwrites migList[i].addr internally).
 */
int DoSwapOut(ProcessAttr *process, uint64_t max_pages, uint8_t threshold)
{
    if (!process || max_pages == 0) {
        return 0;
    }

    struct ProcessManager *manager = GetProcessManager();
    if (!manager || manager->fds.migrate < 0) {
        SMAP_LOGGER_ERROR("Migration device fd not open, cannot swap out.");
        return 0;
    }

    struct MigrateMsg mMsg;
    memset_s(&mMsg, sizeof(mMsg), 0, sizeof(mMsg));
    mMsg.mulMig.pageSize = manager->tracking.pageSize;
    mMsg.mulMig.nrThread = 1;
    mMsg.mulMig.isMulThread = false;

    int maxEntries = MAX_NODES;
    mMsg.migList = (struct MigList *)calloc(maxEntries, sizeof(struct MigList));
    if (!mMsg.migList) {
        SMAP_LOGGER_ERROR("DoSwapOut: migList alloc failed.");
        return 0;
    }

    uint64_t *savedAddrs[MAX_NODES];
    memset_s(savedAddrs, sizeof(savedAddrs), 0, sizeof(savedAddrs));

    uint64_t remaining_budget = max_pages;

    for (int nid = 0; nid < MAX_NODES && nid < SWAP_MAX_NODES && remaining_budget > 0; nid++) {
        if (!ShouldTrackNodeForSwap(process, nid)) {
            continue;
        }

        ActcData *data = process->scanAttr.actcData[nid];
        uint64_t len = process->scanAttr.actcLen[nid];
        ColdTracker *ct = &process->coldState.tracker[nid];
        if (!data || len == 0 || !ct->cold_windows) {
            continue;
        }

        uint64_t node_cold = 0;
        for (uint64_t i = 0; i < ct->page_count && i < len; i++) {
            if (ct->cold_windows[i] >= threshold) {
                node_cold++;
                if (node_cold >= remaining_budget) {
                    break;
                }
            }
        }
        if (node_cold == 0) {
            continue;
        }

        uint64_t *addrs = (uint64_t *)malloc(sizeof(uint64_t) * node_cold);
        if (!addrs) {
            continue;
        }

        uint64_t idx = 0;
        for (uint64_t i = 0; i < ct->page_count && i < len && idx < node_cold; i++) {
            if (ct->cold_windows[i] >= threshold) {
                addrs[idx++] = data[i].addr;
                ct->cold_windows[i] = 0; /* reset once submitted for swap-out */
            }
        }

        mMsg.migList[mMsg.cnt].pid = process->pid;
        mMsg.migList[mMsg.cnt].from = nid;
        mMsg.migList[mMsg.cnt].to = nid;
        mMsg.migList[mMsg.cnt].nr = idx;
        mMsg.migList[mMsg.cnt].addr = addrs;
        savedAddrs[mMsg.cnt] = addrs;
        mMsg.cnt++;
        remaining_budget -= idx;

        if (mMsg.cnt >= maxEntries) {
            break;
        }
    }

    if (mMsg.cnt == 0) {
        free(mMsg.migList);
        return 0;
    }

    int ret = ioctl(manager->fds.migrate, SMAP_MIG_SWAP_OUT, &mMsg);

    int total_swapped = 0;
    for (int i = 0; i < mMsg.cnt; i++) {
        if (mMsg.migList[i].successToUser) {
            uint64_t success = (mMsg.migList[i].nr >= mMsg.migList[i].failedMigNr)
                                   ? (mMsg.migList[i].nr - mMsg.migList[i].failedMigNr)
                                   : 0;
            total_swapped += (int)success;
            SMAP_LOGGER_INFO("pid %d node %d: swapped %lu/%lu pages.",
                             process->pid, mMsg.migList[i].from,
                             success, mMsg.migList[i].nr);
        }
        if (savedAddrs[i]) {
            free(savedAddrs[i]);
        }
    }
    free(mMsg.migList);

    if (ret < 0) {
        SMAP_LOGGER_ERROR("SMAP_MIG_SWAP_OUT ioctl failed: %d", ret);
        process->swapAccounting.swap_out_failures++;
    }

    return total_swapped;
}
