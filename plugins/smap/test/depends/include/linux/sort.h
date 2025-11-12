/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SORT_H
#define _LINUX_SORT_H

#include <linux/types.h>


#ifdef __cplusplus
extern "C" {
#endif

void sort(void *base, size_t num, size_t size,
	  cmp_func_t cmp_func,
	  swap_func_t swap_func);

#ifdef __cplusplus
}
#endif

#endif
