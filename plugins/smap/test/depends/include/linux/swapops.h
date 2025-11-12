// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP3.0 stub function
*/

#ifndef LINUX_SWAPOPS_H
#define LINUX_SWAPOPS_H

#include <linux/xarray.h>
#include <linux/pgtable.h>
#include <linux/mm_types.h>

#define BITS_PER_XA_VALUE	(BITS_PER_LONG - 1)
#define MAX_SWAPFILES_SHIFT	5
#define SWP_TYPE_SHIFT	(BITS_PER_XA_VALUE - MAX_SWAPFILES_SHIFT)
#define SWP_OFFSET_MASK	((1UL << SWP_TYPE_SHIFT) - 1)

static inline unsigned swp_type(swp_entry_t entry)
{
    return (entry.val >> SWP_TYPE_SHIFT);
}

static inline pgoff_t swp_offset(swp_entry_t entry)
{
    return entry.val & SWP_OFFSET_MASK;
}

static inline int is_swap_pte(pte_t pte)
{
    return !pte_none(pte) && !pte_present(pte);
}

static inline swp_entry_t pte_to_swp_entry(pte_t pte)
{
    swp_entry_t arch_entry;
    arch_entry.val = pte.pte;
    return arch_entry;
}

static inline int is_pmd_migration_entry(pmd_t pmd)
{
    return 0;
}
#endif /* LINUX_SWAPOPS_H */
