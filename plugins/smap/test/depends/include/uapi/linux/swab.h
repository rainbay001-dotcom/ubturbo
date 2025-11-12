/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_SWAB_DEPENDS_H
#define _UAPI_LINUX_SWAB_DEPENDS_H

#include <asm/bitsperlong.h>
#include <asm/swab.h>
#include <linux/compiler.h>
#include <linux/types.h>

#define ___constant_swab64(x) ((__u64)(				\
	(((__u64)(x) & (__u64)0x00000000000000ffULL) << 56) | (((__u64)(x) & (__u64)0x000000000000ff00ULL) << 40) |	\
	(((__u64)(x) & (__u64)0x0000000000ff0000ULL) << 24) | (((__u64)(x) & (__u64)0x00000000ff000000ULL) <<  8) |	\
	(((__u64)(x) & (__u64)0x000000ff00000000ULL) >>  8) | (((__u64)(x) & (__u64)0x0000ff0000000000ULL) >> 24) |	\
	(((__u64)(x) & (__u64)0x00ff000000000000ULL) >> 40) | (((__u64)(x) & (__u64)0xff00000000000000ULL) >> 56)))

#define ___constant_swab32(x) ((__u32)(				\
	(((__u32)(x) & (__u32)0x000000ffUL) << 24) | (((__u32)(x) & (__u32)0x0000ff00UL) <<  8) |	\
	(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | (((__u32)(x) & (__u32)0xff000000UL) >> 24)))

#define ___constant_swab16(x) ((__u16)(				\
	(((__u16)(x) & (__u16)0x00ffU) << 8) | (((__u16)(x) & (__u16)0xff00U) >> 8)))

#define __swab64(x) ___constant_swab64(x)
#define __swab32(x) ___constant_swab32(x)
#define __swab16(x) ___constant_swab16(x)

static __always_inline __u64 __swab64p(const __u64 *p)
{
	return __swab64(*p);
}

static __always_inline __u32 __swab32p(const __u32 *p)
{
	return __swab32(*p);
}

static __always_inline unsigned long __swab(const unsigned long y)
{
#if __BITS_PER_LONG == 64
	return __swab64(y);
#else
	return __swab32(y);
#endif
}

static __always_inline __u16 __swab16p(const __u16 *p)
{
	return __swab16(*p);
}

#endif
