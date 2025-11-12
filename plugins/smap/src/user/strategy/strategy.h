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

#ifndef __STRATEGY_H__
#define __STRATEGY_H__

#include "manage/manage.h"
#include "smap_user_log.h"

#define FREE_BYTES_INDEX 3
#define KB_TO_PAGE_SHIFT 2
#define NR_RESERVED_PAGES 16384

static inline void CalcMaxMigrate(uint divisor, uint64_t total, uint64_t *res)
{
    *res = total / divisor;
}
static inline uint32_t NrLocalPage(ProcessAttr *process)
{
    uint32_t pages = 0;
    for (int n = 0; n < GetNrLocalNuma(); n++) {
        if (NotInAttrL1(process, n)) {
            continue;
        }
        pages += process->walkPage.nrPages[n];
    }
    return pages;
}

static inline uint32_t NrRemotePage(ProcessAttr *process)
{
    uint32_t pages = 0;
    for (int n = LOCAL_NUMA_NUM; n < MAX_NODES; n++) {
        if (NotInAttrL2(process, n)) {
            continue;
        }
        pages += process->walkPage.nrPages[n];
    }
    return pages;
}

static inline uint32_t NrTotalPages(ProcessAttr *process)
{
    return NrLocalPage(process) + NrRemotePage(process);
}

static inline int CheckActcDataValid(ProcessAttr *process)
{
    bool nullFlag = true;
    for (int n = 0; n < MAX_NODES; n++) {
        nullFlag &= !process->scanAttr.actcData[n];
    }
    if (nullFlag) {
        SMAP_LOGGER_ERROR("The actcData is null, pid %d stops migrate out.", process->pid);
        return -ENODATA;
    }
    return 0;
}

int RunStrategy(ProcessAttr *process, struct MigList mlist[MAX_NODES][MAX_NODES], size_t mlistSize);
uint64_t GetNrFreePagesByNode(int nid);
uint64_t GetNrFreeHugePagesByNode(int nid);

#endif /* __STRATEGY_H__ */
