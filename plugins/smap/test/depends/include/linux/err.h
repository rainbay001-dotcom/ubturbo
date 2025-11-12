/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_ERR_H
#define _LINUX_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/errno.h>

#define MAX_ERRNO	4095    /* MAX_ERRNO */

#ifndef __ASSEMBLY__

#ifndef unlikely
# define unlikely(x)		__builtin_expect(!!(x), 0)
#endif

#ifndef likely
# define likely(x)		__builtin_expect(!!(x), 1)
#endif

#define IS_ERR_VALUE(x) unlikely((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)

#define __must_check

static inline int __must_check PTR_ERR_OR_ZERO(__force const void *ptr)
{
	return 0;
}

static inline void * __must_check ERR_PTR(long error)
{
	return (void *) error;
}

static inline void * __must_check ERR_CAST(__force const void *ptr)
{
	return (void *) ptr;
}

static inline bool __must_check IS_ERR_OR_NULL(__force const void *ptr)
{
	return unlikely(!ptr) || IS_ERR_VALUE((unsigned long)ptr);
}

#endif

#ifdef __cplusplus
}
#endif

#endif

