/* SPDX-License-Identifier: GPL-2.0-or-later */
/* internal.h: mm/ internal definitions
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */
#ifndef __MM_INTERNAL_H
#define __MM_INTERNAL_H

#include <linux/mm.h>
#include <linux/rmap.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int isolate_lru_page(struct page *page);

#ifdef __cplusplus
}
#endif

#endif	/* __MM_INTERNAL_H */
