/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _UAPI_LINUX_CONST_H
#define _UAPI_LINUX_CONST_H

#ifdef __ASSEMBLY__
#define _AT(T, X)	X
#define _AC(X, Y)	X
#else
#define _AT(T, X)	((T)(X))
#define __AC(X, Y)	(X##Y)
#define _AC(X, Y)	__AC(X, Y)
#endif

#define __KERNEL_DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)

#define _BITUL(x)	(_UL(1) << (x))
#define _BITULL(x)	(_ULL(1) << (x))

#define _UL(x)		(_AC(x, UL))
#define _ULL(x)		(_AC(x, ULL))

#endif
