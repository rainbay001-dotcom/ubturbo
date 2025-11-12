// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: pgtable-generic.c stub.
 */
#include <asm/pgtable-types.h>
#include <linux/mm.h>

pte_t *drivers__pte_offset_map(pmd_t *pmd, unsigned long addr, pmd_t *pmdvalp_)
{
    return NULL;
}

pte_t *__pte_offset_map(pmd_t *pmd, unsigned long addr, pmd_t *pmdvalp_)
{
    return NULL;
}