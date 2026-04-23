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
 * actcData[nid][i].addr stores a bitmap INDEX (0,1,2,...). The kernel
 * module's SMAP_MIG_SWAP_OUT ioctl reuses the same infrastructure as
 * SMAP_MIG_MIGRATE:
 *   1. copy_from_user the MigrateMsg with bitmap indices
 *   2. convert_pos_to_paddr_sorted() converts indices to physical addresses
 *   3. Collect folios from physical addresses
 *   4. Call fp_reclaim_pages() to force kernel swap-out
 *
 * This gives per-page granularity with minimal code change.
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
#include "swap_out.h"

void ClosePidfd(ProcessAttr *process)
{
    if (!process) {
        return;
    }
    if (process->swap_pidfd >= 0) {
        close(process->swap_pidfd);
        process->swap_pidfd = -1;
    }
}

/*
 * DoSwapOut - Swap cold pages from L2 to NVMe via kernel ioctl.
 *
 * @process:     Target process
 * @cold_indices: Bitmap indices of cold pages (from SelectSwapCandidates)
 * @nr_cold:     Number of cold page indices
 * @batch_size:  Unused in ioctl mode (kept for API compatibility)
 *
 * Builds a MigrateMsg with bitmap indices and calls ioctl(SMAP_MIG_SWAP_OUT).
 * The kernel converts indices to PA, isolates folios, and calls reclaim_pages()
 * to force them to the configured swap device.
 */
int DoSwapOut(ProcessAttr *process, uint64_t *cold_indices, uint64_t nr_cold, uint32_t batch_size)
{
    if (!process || !cold_indices || nr_cold == 0) {
        return 0;
    }

    struct ProcessManager *manager = GetProcessManager();
    if (manager->fds.migrate < 0) {
        SMAP_LOGGER_ERROR("Migration device fd not open, cannot swap out.");
        return 0;
    }

    /* Build MigrateMsg with cold page bitmap indices — same format as migration */
    struct MigrateMsg mMsg;
    memset_s(&mMsg, sizeof(mMsg), 0, sizeof(mMsg));
    mMsg.mulMig.pageSize = manager->tracking.pageSize;
    mMsg.mulMig.nrThread = 1;
    mMsg.mulMig.isMulThread = false;

    /*
     * Allocate space for mig_list entries per NUMA node.
     * Supports both L2→L3 (standard tiered) and L1→L3 (no CXL) scenarios.
     *
     * The kernel's init_migrate_list_addr() overwrites mig_list[i].addr
     * with a kernel pointer (see mig_init.c:112-113), then
     * __ioctl_swap_out() copies the whole mig_list back to user space
     * (mig_init.c:671-672). So after the ioctl, mMsg.migList[i].addr
     * contains a kernel address that free() would crash on.
     * We keep a shadow array of our malloc'd pointers to free instead.
     */
    int maxEntries = MAX_NODES;
    mMsg.migList = (struct MigList *)calloc(maxEntries, sizeof(struct MigList));
    if (!mMsg.migList) {
        SMAP_LOGGER_ERROR("DoSwapOut: migList alloc failed.");
        return 0;
    }
    uint64_t *savedAddrs[MAX_NODES];
    memset_s(savedAddrs, sizeof(savedAddrs), 0, sizeof(savedAddrs));

    /* Group cold indices by source NUMA node (L1 or L2) */
    bool has_l2 = (process->remoteNumaCnt > 0);
    int nrLocal = GetNrLocalNuma();

    for (int nid = 0; nid < MAX_NODES; nid++) {
        if (nid >= SWAP_MAX_NODES) {
            break;
        }
        /* Determine if this node should be tracked for swap */
        if (has_l2) {
            /* Standard tiered: swap from L2 nodes only */
            if (nid < nrLocal || NotInAttrL2(process, nid)) {
                continue;
            }
        } else {
            /* L1+L3 only: swap from L1 nodes */
            if (nid >= nrLocal || NotInAttrL1(process, nid)) {
                continue;
            }
        }

        ActcData *data = process->scanAttr.actcData[nid];
        uint64_t len = process->scanAttr.actcLen[nid];
        if (!data || len == 0) {
            continue;
        }

        /* Count cold pages on this node */
        uint64_t node_cold = 0;
        ColdTracker *ct = &process->coldState.tracker[nid];
        if (!ct->cold_windows) {
            continue;
        }
        for (uint64_t i = 0; i < ct->page_count && i < len; i++) {
            if (ct->cold_windows[i] >= ct->threshold) {
                node_cold++;
            }
        }

        if (node_cold == 0) {
            continue;
        }

        /* Build addr array with bitmap indices for this node's cold pages */
        uint64_t *addrs = (uint64_t *)malloc(sizeof(uint64_t) * node_cold);
        if (!addrs) {
            continue;
        }

        uint64_t idx = 0;
        for (uint64_t i = 0; i < ct->page_count && i < len && idx < node_cold; i++) {
            if (ct->cold_windows[i] >= ct->threshold) {
                addrs[idx++] = data[i].addr; /* bitmap index, not PA */
            }
        }

        mMsg.migList[mMsg.cnt].pid = process->pid;
        mMsg.migList[mMsg.cnt].from = nid;
        mMsg.migList[mMsg.cnt].to = nid; /* to is unused for swap-out */
        mMsg.migList[mMsg.cnt].nr = idx;
        mMsg.migList[mMsg.cnt].addr = addrs;
        savedAddrs[mMsg.cnt] = addrs; /* shadow: kernel will clobber .addr */
        mMsg.cnt++;

        if (mMsg.cnt >= maxEntries) {
            break;
        }
    }

    if (mMsg.cnt == 0) {
        free(mMsg.migList);
        return 0;
    }

    /* Issue SMAP_MIG_SWAP_OUT ioctl — kernel handles index→PA→folio→reclaim */
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
        /* Free our shadow pointer, NOT mMsg.migList[i].addr — the kernel
         * overwrote that field with a (now-freed) kernel address during
         * the ioctl. free()ing it would segfault in libc. */
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
