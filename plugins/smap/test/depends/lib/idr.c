/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/idr.h>
#include <linux/types.h>

void ida_free(struct ida *ida, unsigned int id)
{
}

int ida_alloc_range(struct ida *ida, unsigned int min, unsigned int max,
                    gfp_t gfp)
{
    return 0;
}

void ida_destroy(struct ida *ida)
{
}