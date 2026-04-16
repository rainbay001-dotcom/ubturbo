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
    for (int i = 0; i < REMOTE_NUMA_NUM; i++) {
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
    for (int i = 0; i < REMOTE_NUMA_NUM; i++) {
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

void UpdateColdWindowCounters(ProcessAttr *process)
{
    if (!process) {
        return;
    }

    int nrLocal = GetNrLocalNuma();
    for (int nid = nrLocal; nid < MAX_NODES; nid++) {
        if (NotInAttrL2(process, nid)) {
            continue;
        }

        int tracker_idx = nid - LOCAL_NUMA_NUM;
        if (tracker_idx < 0 || tracker_idx >= REMOTE_NUMA_NUM) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[tracker_idx];
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

    /* First pass: count candidates across all L2 nodes */
    int nrLocal = GetNrLocalNuma();
    for (int nid = nrLocal; nid < MAX_NODES; nid++) {
        if (NotInAttrL2(process, nid)) {
            continue;
        }

        int tracker_idx = nid - LOCAL_NUMA_NUM;
        if (tracker_idx < 0 || tracker_idx >= REMOTE_NUMA_NUM) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[tracker_idx];
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

    /* Second pass: collect physical addresses */
    uint64_t idx = 0;
    for (int nid = nrLocal; nid < MAX_NODES; nid++) {
        if (NotInAttrL2(process, nid)) {
            continue;
        }

        int tracker_idx = nid - LOCAL_NUMA_NUM;
        if (tracker_idx < 0 || tracker_idx >= REMOTE_NUMA_NUM) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[tracker_idx];
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
