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
 * Cold Page Tracker: Multi-window consecutive zero-frequency detection.
 *
 * Tracks per-page cold window counters on ALL NUMA nodes (L1 and L2).
 * This supports two deployment scenarios:
 *   - L1 + L2 + L3: cold L2 pages → NVMe (standard tiered memory)
 *   - L1 + L3 only: cold L1 pages → NVMe directly (no CXL/PMEM)
 *
 * The tracker array is indexed by NUMA node ID (0..MAX_NODES-1),
 * covering both local (L1) and remote (L2) nodes.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "smap_user_log.h"
#include "securec.h"
#include "manage/manage.h"
#include "cold_tracker.h"

void InitProcessColdState(ProcessColdState *state)
{
    if (!state) {
        return;
    }
    for (int i = 0; i < SWAP_MAX_NODES; i++) {
        state->tracker[i].page_count = 0;
        state->tracker[i].cold_windows = NULL;
        state->tracker[i].threshold = SWAP_DEFAULT_COLD_WINDOW_THRESHOLD;
    }
}

void DestroyColdTracker(ProcessColdState *state)
{
    if (!state) {
        return;
    }
    for (int i = 0; i < SWAP_MAX_NODES; i++) {
        if (state->tracker[i].cold_windows) {
            free(state->tracker[i].cold_windows);
            state->tracker[i].cold_windows = NULL;
        }
        state->tracker[i].page_count = 0;
    }
}

static int RebuildTracker(ColdTracker *ct, uint64_t new_count)
{
    if (ct->cold_windows) {
        free(ct->cold_windows);
        ct->cold_windows = NULL;
    }
    ct->page_count = 0;

    if (new_count == 0) {
        return 0;
    }

    ct->cold_windows = (uint8_t *)calloc(new_count, sizeof(uint8_t));
    if (!ct->cold_windows) {
        SMAP_LOGGER_ERROR("ColdTracker: failed to allocate %lu bytes", new_count);
        return -ENOMEM;
    }
    ct->page_count = new_count;
    return 0;
}

/*
 * Check if a NUMA node should be tracked for cold page swap.
 *
 * Two modes:
 *   - If process has L2 nodes: track L2 nodes only (standard tiered memory)
 *   - If process has NO L2 nodes: track L1 nodes (L1+L3 only deployment)
 */
static bool ShouldTrackNode(ProcessAttr *process, int nid)
{
    int nrLocal = GetNrLocalNuma();
    bool has_l2 = (process->remoteNumaCnt > 0);

    if (has_l2) {
        /* Standard tiered mode: only track L2 nodes */
        if (nid < nrLocal) {
            return false;
        }
        return InAttrL2(process, nid);
    } else {
        /* L1+L3 only mode: track L1 nodes */
        if (nid >= nrLocal) {
            return false;
        }
        return InAttrL1(process, nid);
    }
}

void UpdateColdWindowCounters(ProcessAttr *process)
{
    if (!process) {
        return;
    }

    for (int nid = 0; nid < MAX_NODES; nid++) {
        if (!ShouldTrackNode(process, nid)) {
            continue;
        }
        if (nid >= SWAP_MAX_NODES) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[nid];
        ActcData *data = process->scanAttr.actcData[nid];
        uint64_t len = process->scanAttr.actcLen[nid];

        if (!data || len == 0) {
            continue;
        }

        /* Rebuild if page set size changed */
        if (ct->page_count != len) {
            if (RebuildTracker(ct, len) != 0) {
                continue;
            }
        }

        /* Update counters: freq=0 increments, any access resets */
        for (uint64_t i = 0; i < len; i++) {
            if (data[i].freq == 0 && !data[i].isWhiteListPage) {
                if (ct->cold_windows[i] < COLD_TRACKER_MAX_WINDOWS) {
                    ct->cold_windows[i]++;
                }
            } else {
                ct->cold_windows[i] = 0;
            }
        }
    }
}

uint64_t SelectSwapCandidates(ProcessAttr *process, uint64_t **addrs_out, uint8_t threshold)
{
    if (!process || !addrs_out) {
        return 0;
    }

    *addrs_out = NULL;
    uint64_t total_candidates = 0;

    /* First pass: count candidates across all tracked nodes */
    for (int nid = 0; nid < MAX_NODES; nid++) {
        if (!ShouldTrackNode(process, nid)) {
            continue;
        }
        if (nid >= SWAP_MAX_NODES) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[nid];
        if (!ct->cold_windows || ct->page_count == 0) {
            continue;
        }

        for (uint64_t i = 0; i < ct->page_count; i++) {
            if (ct->cold_windows[i] >= threshold) {
                total_candidates++;
            }
        }
    }

    if (total_candidates == 0) {
        return 0;
    }

    /* Allocate output array */
    *addrs_out = (uint64_t *)calloc(total_candidates, sizeof(uint64_t));
    if (!*addrs_out) {
        SMAP_LOGGER_ERROR("SelectSwapCandidates: alloc failed for %lu entries", total_candidates);
        return 0;
    }

    /* Second pass: collect bitmap indices */
    uint64_t idx = 0;
    for (int nid = 0; nid < MAX_NODES; nid++) {
        if (!ShouldTrackNode(process, nid)) {
            continue;
        }
        if (nid >= SWAP_MAX_NODES) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[nid];
        ActcData *data = process->scanAttr.actcData[nid];

        if (!ct->cold_windows || !data) {
            continue;
        }

        for (uint64_t i = 0; i < ct->page_count && idx < total_candidates; i++) {
            if (ct->cold_windows[i] >= threshold) {
                (*addrs_out)[idx++] = data[i].addr;
            }
        }
    }

    return idx;
}
