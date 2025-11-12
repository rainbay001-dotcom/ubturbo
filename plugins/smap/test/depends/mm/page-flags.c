/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/page-flags.h>
#include <linux/gfp_types.h>

struct page *compound_head(struct page *page)
{
	return page;
}