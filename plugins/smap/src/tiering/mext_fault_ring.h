/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: SMAP MEXT kernel fault/eviction mmap ring & proactive prefault
 */

#ifndef _SRC_TIERING_MEXT_FAULT_RING_H
#define _SRC_TIERING_MEXT_FAULT_RING_H

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/types.h>

#include "mext_ring_abi.h"

int mext_fault_ring_init(void);
void mext_fault_ring_exit(void);

int mext_fault_ring_mmap(struct file *filp, struct vm_area_struct *vma);

u64 mext_next_swapout_epoch(void);
void mext_record_eviction(pid_t pid, u64 vpn, u64 epoch);

int mext_ioctl_prefault(void __user *argp);
int mext_ioctl_ctrl(void __user *argp);

#endif /* _SRC_TIERING_MEXT_FAULT_RING_H */
