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
 * Tracks per-page cold window counters on the nodes targeted for swap.
 * A page is a swap candidate after cold_window_threshold consecutive
 * scan windows with freq=0.
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
 * Returns true if nid is a node that should be tracked for cold page swap.
 * When the process has L2 nodes (L1+L2+L3 topology), only L2 nodes are tracked.
 * Without L2 (L1+L3 only), L1 nodes are tracked for direct swap to NVMe.
 */
static bool ShouldTrackNode(ProcessAttr *process, int nid)
{
    if (process->remoteNumaCnt > 0) {
        return InAttrL2(process, nid);
    }
    return (nid < GetNrLocalNuma()) && InAttrL1(process, nid);
}

void UpdateColdWindowCounters(ProcessAttr *process)
{
    if (!process) {
        return;
    }

    for (int nid = 0; nid < SWAP_MAX_NODES; nid++) {
        if (!ShouldTrackNode(process, nid)) {
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
