/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "smap_user_log.h"
#include "securec.h"
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

bool ShouldTrackNodeForSwap(ProcessAttr *process, int nid)
{
    if (!process || nid < 0 || nid >= SWAP_MAX_NODES) {
        return false;
    }

    int nrLocal = GetNrLocalNuma();
    bool has_l2 = (process->remoteNumaCnt > 0);

    if (has_l2) {
        if (nid < nrLocal) {
            return false;
        }
        return InAttrL2(process, nid);
    } else {
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

    for (int nid = 0; nid < MAX_NODES && nid < SWAP_MAX_NODES; nid++) {
        if (!ShouldTrackNodeForSwap(process, nid)) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[nid];
        ActcData *data = process->scanAttr.actcData[nid];
        uint64_t len = process->scanAttr.actcLen[nid];

        if (!data || len == 0) {
            continue;
        }

        if (ct->page_count != len) {
            if (RebuildTracker(ct, len) != 0) {
                continue;
            }
        }

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

uint64_t CountColdSwapCandidates(ProcessAttr *process, uint8_t threshold)
{
    if (!process) {
        return 0;
    }

    uint64_t total_candidates = 0;
    for (int nid = 0; nid < MAX_NODES && nid < SWAP_MAX_NODES; nid++) {
        if (!ShouldTrackNodeForSwap(process, nid)) {
            continue;
        }

        ColdTracker *ct = &process->coldState.tracker[nid];
        uint64_t len = process->scanAttr.actcLen[nid];
        if (!ct->cold_windows || ct->page_count == 0) {
            continue;
        }

        for (uint64_t i = 0; i < ct->page_count && i < len; i++) {
            if (ct->cold_windows[i] >= threshold) {
                total_candidates++;
            }
        }
    }
    return total_candidates;
}
