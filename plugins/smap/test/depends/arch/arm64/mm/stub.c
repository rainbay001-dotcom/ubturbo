// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: arm64 mm stub
 */

#include <asm/pgtable-types.h>

pte_t g_tmp_pte = { 0 };

pte_t ptep_get(pte_t *ptep)
{
    return g_tmp_pte;
}
