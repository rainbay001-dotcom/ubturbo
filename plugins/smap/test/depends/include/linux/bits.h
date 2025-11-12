/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_BITS_H
#define __LINUX_BITS_H

#include <asm/bitsperlong.h>
#include <linux/limits.h>

#define BIT_ULL(nr)		(ULL(1) << (nr))

#define BITS_PER_BYTE 8

#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)

#include <linux/build_bug.h>
#define GENMASK_INPUT_CHECK(h, l) \
    (BUILD_BUG_ON_ZERO(__builtin_choose_expr( \
        __is_constexpr((l) > (h)), (l) > (h), 0)))

#define __GENMASK_ULL(h, l) \
    (((~ULL(0)) - (ULL(1) << (l)) + 1) & \
    (~ULL(0) >> (BITS_PER_LONG_LONG - 1 - (h))))
#define GENMASK_ULL(h, l) \
    (GENMASK_INPUT_CHECK(h, l) + __GENMASK_ULL(h, l))

#define dsb(ishst)
#define __tlbi(aside1is, asid)
#define __tlbi_user(aside1is, asid)

#define ASID(mm) 0
#define __TLBI_VADDR(addr, asid) 0

#endif	/* __LINUX_BITS_H */
