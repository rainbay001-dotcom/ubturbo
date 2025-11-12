/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdlib.h>
#include <linux/sort.h>


void sort(void *base, size_t num, size_t size,
	  cmp_func_t cmp_func,
	  swap_func_t swap_func)
{
	qsort(base, num, size, cmp_func);
}
