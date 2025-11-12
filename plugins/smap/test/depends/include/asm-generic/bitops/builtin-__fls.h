// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP3.0 stub function
*/

#ifndef ASM64_GENERIC_BITOPS_BUILTIN___FLS_H_
#define ASM64_GENERIC_BITOPS_BUILTIN___FLS_H_

static __always_inline unsigned long __fls(unsigned long word)
{
    return (sizeof(word) * 8) - 1 - __builtin_clzl(word);
}

#endif
