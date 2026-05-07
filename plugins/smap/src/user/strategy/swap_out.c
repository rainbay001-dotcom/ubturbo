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

/*
 * Swap-Out Executor: Per-page cold page swap via kernel ioctl.
 *
 * actcData[nid][i].addr stores a bitmap INDEX. The kernel SMAP_MIG_SWAP_OUT
 * ioctl reuses the migrate_msg format:
 *   1. copy_from_user the MigrateMsg with bitmap indices
 *   2. convert_pos_to_paddr_sorted() converts indices to physical addresses
 *   3. Collect folios from physical addresses
 *   4. Call fp_reclaim_pages() to force kernel swap-out
 *
 * The kernel overwrites mig_list[i].addr with a kernel pointer during the
 * ioctl. We keep shadow pointers to our malloc'd buffers and free those.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "smap_user_log.h"
#include "securec.h"
#include "manage/manage.h"
#include "manage/smap_ioctl.h"
#include "cold_tracker.h"
#include "swap_out.h"

int DoSwapOut(ProcessAttr *process)
{
    if (!process) {
        return 0;
    }

    struct ProcessManager *manager = GetProcessManager();
    if (manager->fds.migrate < 0) {
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

    bool has_l2 = HasL2ScanData(process);
    int nrLocal = GetNrLocalNuma();

    for (int nid = 0; nid < SWAP_MAX_NODES; nid++) {
        if (has_l2) {
            if (NotInAttrL2(process, nid)) {
                continue;
            }
        } else {
            /* swapMode or no L2: swap directly from L1 nodes */
            if (nid >= nrLocal || NotInAttrL1(process, nid)) {
                continue;
            }
        }

        ActcData *data = process->scanAttr.actcData[nid];
        uint64_t len = process->scanAttr.actcLen[nid];
        if (!data || len == 0) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[nid];
        if (!ct->cold_windows) {
            continue;
        }

        uint64_t node_cold = 0;
        for (uint64_t i = 0; i < ct->page_count && i < len; i++) {
            if (ct->cold_windows[i] >= ct->threshold) {
                node_cold++;
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
            if (ct->cold_windows[i] >= ct->threshold) {
                addrs[idx++] = data[i].addr;
            }
        }

        mMsg.migList[mMsg.cnt].pid = process->pid;
        mMsg.migList[mMsg.cnt].from = nid;
        mMsg.migList[mMsg.cnt].to = nid;
        mMsg.migList[mMsg.cnt].nr = idx;
        mMsg.migList[mMsg.cnt].addr = addrs;
        savedAddrs[mMsg.cnt] = addrs;
        mMsg.cnt++;

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
            uint64_t success = mMsg.migList[i].nr - mMsg.migList[i].failedMigNr;
            total_swapped += success;
            SMAP_LOGGER_INFO("pid %d node %d: swapped %lu/%lu pages.",
                             process->pid, mMsg.migList[i].from,
                             success, mMsg.migList[i].nr);
        }
        /* Free our shadow pointer — the kernel overwrote .addr with a kernel
         * address during the ioctl, so free()ing .addr directly would segfault. */
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
