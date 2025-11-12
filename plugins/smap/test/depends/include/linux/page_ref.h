/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PAGE_REF_H
#define _LINUX_PAGE_REF_H

static inline int page_ref_count(const struct page *page)
{
	return 0;
}

static inline int page_count(struct page *page)
{
	return 0;
}

static inline bool folio_try_get(struct folio *folio)
{
    return true;
}

#endif
