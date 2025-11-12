/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef HAM_BASIC_H
#define HAM_BASIC_H

#include <asm/pgtable-types.h>
#include <linux/pgtable.h>
#include <linux/mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

pte_t kernel_huge_ptep_get(pte_t *ptep);

struct mm_struct *find_get_mm_by_vpid(pid_t pid);

#ifdef __cplusplus
}
#endif
#endif