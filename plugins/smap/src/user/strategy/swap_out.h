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
 * DoSwapOut - Swap cold pages to NVMe via kernel ioctl (SMAP_MIG_SWAP_OUT).
 *
 * Walks coldState.tracker for @process, collects cold page bitmap indices,
 * and issues the ioctl. The kernel converts indices to PA, isolates folios,
 * and calls reclaim_pages() to force them to the configured swap device.
 *
 * Returns: number of pages successfully swapped out.
 */
int DoSwapOut(ProcessAttr *process);

#endif /* __SWAP_OUT_H__ */
