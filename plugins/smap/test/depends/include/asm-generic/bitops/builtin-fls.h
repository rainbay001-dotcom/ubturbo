// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP3.0 stub function
*/

#ifndef ASM64_GENERIC_BITOPS_BUILTIN_FLS_H_
#define ASM64_GENERIC_BITOPS_BUILTIN_FLS_H_

static __always_inline int fls(unsigned int val)
{
    return val ? (sizeof(val) * 8 - __builtin_clz(val)) : 0;
}

#endif
