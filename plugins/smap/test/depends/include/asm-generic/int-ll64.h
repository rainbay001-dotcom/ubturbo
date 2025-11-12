/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ASM_DEPENDS_GENERIC_INT_LL64_H
#define _ASM_DEPENDS_GENERIC_INT_LL64_H

#include <uapi/asm-generic/int-ll64.h>

#ifndef __ASSEMBLY__

typedef __u64 u64;
#define U64_C(x) x ## ULL
typedef __s64 s64;
#define S64_C(x) x ## LL
typedef __u32 u32;
#define U32_C(x) x ## U
typedef __s32 s32;
#define S32_C(x) x
typedef __u16 u16;
#define U16_C(x) x ## U
typedef __s16 s16;
#define S16_C(x) x
typedef __u8  u8;
#define U8_C(x)  x ## U
typedef __s8  s8;
#define S8_C(x)  x

#else

#define U64_C(x) x
#define S64_C(x) x
#define U32_C(x) x
#define S32_C(x) x
#define U16_C(x) x
#define S16_C(x) x
#define U8_C(x)  x
#define S8_C(x)  x

#endif

#endif
