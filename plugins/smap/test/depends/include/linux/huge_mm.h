// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP3.0 stub function
*/

#ifndef LINUX_HUGE_MM_H
#define LINUX_HUGE_MM_H

#include <linux/mm_types.h>

#include <linux/fs.h> /* only for vma_is_dax() */

static inline int is_swap_pmd(pmd_t pmd)
{
    return pmd.pmd != 0;
}


#endif /* LINUX_HUGE_MM_H */
