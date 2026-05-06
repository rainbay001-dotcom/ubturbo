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
 * Cold Page Tracker: bitmap-indexed consecutive zero-frequency detection.
 *
 * hist_cold[nid][addr] counts how many consecutive scan cycles the page at
 * bitmap index addr has had freq==0. Indexed by ActcData.addr (bitmap index),
 * not by position in actcData[]. This avoids stale counters when actcData
 * entries shift between cycles. Allocated once at process add time (Plan A).
 */

#include <stdlib.h>
#include <stdint.h>

#include "smap_user_log.h"
#include "manage/manage.h"
#include "cold_tracker.h"

void InitHistCold(ProcessAttr *process)
{
    if (!process) {
        return;
    }
    for (int nid = 0; nid < SWAP_MAX_NODES; nid++) {
        process->hist_cold[nid] = NULL;
        process->hist_cold_len[nid] = 0;
        uint64_t len = process->walkPage.nrPages[nid];
        if (len == 0) {
            continue;
        }
        process->hist_cold[nid] = (uint8_t *)calloc(len, sizeof(uint8_t));
        if (!process->hist_cold[nid]) {
            SMAP_LOGGER_ERROR("InitHistCold: calloc failed for nid %d len %lu", nid, len);
            continue;
        }
        process->hist_cold_len[nid] = len;
    }
}

void FreeHistCold(ProcessAttr *process)
{
    if (!process) {
        return;
    }
    for (int nid = 0; nid < SWAP_MAX_NODES; nid++) {
        if (process->hist_cold[nid]) {
            free(process->hist_cold[nid]);
            process->hist_cold[nid] = NULL;
        }
        process->hist_cold_len[nid] = 0;
    }
}

bool HasL2ScanData(ProcessAttr *process)
{
    if (process->remoteNumaCnt == 0) {
        return false;
    }
    for (int nid = 0; nid < SWAP_MAX_NODES; nid++) {
        if (InAttrL2(process, nid) && process->scanAttr.actcLen[nid] > 0) {
            return true;
        }
    }
    return false;
}

static bool ShouldTrackNode(ProcessAttr *process, int nid)
{
    if (HasL2ScanData(process)) {
        return InAttrL2(process, nid);
    }
    return (nid < GetNrLocalNuma()) && InAttrL1(process, nid);
}

void UpdateHistCold(ProcessAttr *process)
{
    if (!process) {
        return;
    }
    for (int nid = 0; nid < SWAP_MAX_NODES; nid++) {
        if (!ShouldTrackNode(process, nid)) {
            continue;
        }
        ActcData *data = process->scanAttr.actcData[nid];
        uint64_t len = process->scanAttr.actcLen[nid];
        uint8_t *hist = process->hist_cold[nid];
        uint64_t hist_len = process->hist_cold_len[nid];

        if (!data || len == 0 || !hist) {
            continue;
        }
        for (uint64_t i = 0; i < len; i++) {
            uint64_t addr = data[i].addr;
            if (addr >= hist_len) {
                continue;
            }
            if (data[i].freq == 0 && !data[i].isWhiteListPage) {
                if (hist[addr] < UINT8_MAX) {
                    hist[addr]++;
                }
            } else {
                hist[addr] = 0;
            }
        }
    }
}
