/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_BITOPS_H
#define _LINUX_BITOPS_H

#include <linux/types.h>
#include <linux/swab.h>
#include <asm-generic/bitsperlong.h>
#include <asm/bitops.h>
#include <linux/kernel.h>
#include <linux/bits.h>

#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)

#define BITS_PER_TYPE(type)	(sizeof(type) * BITS_PER_BYTE)

#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_TYPE(long))

#define bitop(op, nr, addr)						\
    op(nr, addr)

#define test_bit(nr, addr)		bitop(_test_bit, nr, addr)

#define CLOCK_MONOTONIC 1

static bool _test_bit(unsigned long nr, const volatile unsigned long *addr)
{
    return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG - 1)));
}

#endif
