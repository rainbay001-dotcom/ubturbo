/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/vmalloc.h>
#include <linux/stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void *vmalloc(unsigned long size)
{
	return malloc(size);
}

void *vzalloc(unsigned long size)
{
	return calloc(1, size);
}

void vfree(void *ptr)
{
	return free(ptr);
}

#ifdef __cplusplus
}
#endif