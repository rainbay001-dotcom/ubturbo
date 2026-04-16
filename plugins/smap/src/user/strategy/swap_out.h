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
 * Option A: ioctl-based swap-out via kernel NVMe swap module.
 *
 * Unlike Option B (process_madvise), this path sends physical addresses
 * directly to the kernel via ioctl(SMAP_SWAP_OUT), bypassing the need
 * for PA->VA conversion and pidfd.  The kernel module handles the bio
 * I/O to NVMe and slot management internally.
 */
int DoSwapOut(ProcessAttr *process, uint64_t *phys_addrs, uint64_t nr_pages, uint32_t batch_size);

#endif /* __SWAP_OUT_H__ */
