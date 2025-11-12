// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP3.0 stub function
*/

#ifndef ASM64_GENERIC_RWONCE_H
#define ASM64_GENERIC_RWONCE_H

#include <linux/compiler_types.h>

#define __READ_ONCE(x)  (x)
#define READ_ONCE(x) (x)

#define __WRITE_ONCE(x, val)                        \
do {                                    \
    (x) = (val);                \
} while (0)

#define WRITE_ONCE(x, val)                        \
do {                                    \
    __WRITE_ONCE(x, val);                        \
} while (0)

#endif
