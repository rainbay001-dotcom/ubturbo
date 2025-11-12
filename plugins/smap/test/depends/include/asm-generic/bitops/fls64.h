// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP3.0 stub function
*/

#ifndef ASM64_GENERIC_BITOPS_FLS64_H_
#define ASM64_GENERIC_BITOPS_FLS64_H_

#include <asm/types.h>

static __always_inline int fls64(__u64 x)
{
    if (x == 0) {
        return 0;
    }
    return __fls(x) + 1;
}

#endif
