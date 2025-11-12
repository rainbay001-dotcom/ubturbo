/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/slab.h>

void *kmalloc(size_t size, gfp_t flags)
{
    return malloc(size);
}

void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
    return malloc(n * size);
}

void *kzalloc(size_t size, gfp_t flags)
{
	return calloc(1, size);
}

void *kcalloc(size_t n, size_t size, gfp_t flags)
{
	return kzalloc(n * size, flags);
}

void *kvcalloc(size_t n, size_t size, gfp_t flags)
{
    return calloc(n, size);
}

void kfree(void *p)
{
    free(p);
}

void kvfree(void *p)
{
    free(p);
}
