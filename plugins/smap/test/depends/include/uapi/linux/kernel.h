/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_KERNEL_H
#define _UAPI_LINUX_KERNEL_H

// Aligns a value to the specified alignment using a mask
#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)

// Rounds up the division of n by d
#define __KERNEL_DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

// Aligns a value to the specified mask
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))

#endif /* _UAPI_LINUX_KERNEL_H */
