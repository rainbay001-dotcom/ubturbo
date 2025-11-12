// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/minmax.h>

#ifdef CONFIG_64BIT
#define DEPENDS_LEN 64
#else
#define DEPENDS_LEN 32
#endif

static unsigned long depends_func(const unsigned long *num1, const unsigned long *num2,
		unsigned long z, unsigned long index_s, unsigned long x, unsigned long depends_f)
{
	unsigned long val2;
	unsigned long m;
	unsigned long val1;
	unsigned long begin_idx = index_s;
	val1 = num1[begin_idx / DEPENDS_LEN];
	if (num2 != 0) {
		val1 = val1 & num2[begin_idx / DEPENDS_LEN];
	}
	val1 = val1 ^ x;

	m = BITMAP_FIRST_WORD_MASK(begin_idx);
	if (depends_f != 0) {
		m = swab(m);
	}
	val1 = val1 & m;
	begin_idx = round_down(begin_idx, DEPENDS_LEN);

	while (val1 == 0UL) {
		begin_idx += DEPENDS_LEN;
		if (begin_idx >= z) {
			return z;
	}

		val1 = num1[begin_idx / DEPENDS_LEN];
		if (num2) {
			val1 = val1 & num2[begin_idx / DEPENDS_LEN];
	}
		val1 = val1 ^ x;
	}

	if (depends_f != 0)
		val1 = swab(val1);

	val2 = __builtin_ctzl(val1) + begin_idx;
	if (z >= val2) {
		return val2;
	}
	return z;
}

#ifndef find_next_bit

unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	return depends_func(addr, NULL, size, offset, 0UL, 0);
}
#endif
