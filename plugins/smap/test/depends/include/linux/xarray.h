/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _LINUX_XARRAY_H
#define _LINUX_XARRAY_H

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/types.h>

static inline bool xa_is_internal(const void *entry)
{
	return ((unsigned long)entry & 3) == 2;
}

static inline void *xa_mk_internal(unsigned long v)
{
	return (void *)((v << 2) | 2);
}

#define BITS_PER_XA_VALUE	(BITS_PER_LONG - 1)

static inline bool xa_is_node(const void *entry)
{
	return xa_is_internal(entry) && (unsigned long)entry > 4096;
}

#define XA_ZERO_ENTRY		xa_mk_internal(257)

static inline bool xa_is_zero(const void *entry)
{
	return unlikely(entry == XA_ZERO_ENTRY);
}

#endif
