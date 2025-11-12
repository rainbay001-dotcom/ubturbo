/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: linux memory.h stub
 */
#ifndef LINUX_MEMORY_H
#define LINUX_MEMORY_H

#include <linux/mm.h>
#include <asm/sparsemem.h>

#define MIN_MEMORY_BLOCK_SIZE     (1UL << SECTION_SIZE_BITS)

#define MEM_ONLINE              (1<<0)
#define MEM_GOING_OFFLINE       (1<<1)
#define MEM_OFFLINE             (1<<2)
#define MEM_GOING_ONLINE        (1<<3)
#define MEM_CANCEL_ONLINE       (1<<4)
#define MEM_CANCEL_OFFLINE      (1<<5)

#ifdef __cplusplus
extern "C" {
#endif

extern int pfn_to_nid(unsigned long pfn);

struct memory_notify {
        unsigned long start_pfn;
        unsigned long nr_pages;
        int status_change_nid_normal;
        int status_change_nid;
};

extern int register_memory_notifier(struct notifier_block *nb);
extern void unregister_memory_notifier(struct notifier_block *nb);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_MEMORY_H */
