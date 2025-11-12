/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_MM_INLINE_H
#define LINUX_MM_INLINE_H

#include <linux/mm_types.h>

static inline int page_is_file_lru(struct page *page)
{
	return 0;
}

#endif
