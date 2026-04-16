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
#ifndef __SWAP_OUT_H__
#define __SWAP_OUT_H__

#include "manage/manage.h"

/*
 * DoSwapOut - Swap cold L2 pages to NVMe via process_madvise(MADV_PAGEOUT).
 *
 * @process:     Target process
 * @cold_indices: Bitmap indices of cold pages (from SelectSwapCandidates).
 *                NOTE: These are NOT physical addresses. They are indices
 *                into the kernel's access tracking bitmap. DoSwapOut does
 *                NOT use these for addressing — it uses /proc/pid/numa_maps
 *                to find L2 VMA ranges and advises MADV_PAGEOUT on them.
 *                The cold_indices count drives the swap decision.
 * @nr_cold:     Number of cold page indices (used for decision threshold)
 * @batch_size:  Max iovec entries per process_madvise call
 *
 * Returns: number of pages advised for swap-out (approximate)
 */
int DoSwapOut(ProcessAttr *process, uint64_t *cold_indices, uint64_t nr_cold, uint32_t batch_size);
void ClosePidfd(ProcessAttr *process);

#endif /* __SWAP_OUT_H__ */
