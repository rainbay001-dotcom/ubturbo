/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_GFP_H
#define __LINUX_GFP_H

#include <linux/types.h>
#include <linux/gfp_types.h>
#include <linux/mmzone.h>
#include <linux/nodemask.h>

#define ___GFP_HARDWALL		0x100000u
#define ___GFP_THISNODE		0x200000u
#define ___GFP_HIGHMEM		0x02u
#define ___GFP_NOWARN		0x2000u
#define ___GFP_NOMEMALLOC	0x80000u
#define ___GFP_DIRECT_RECLAIM	0x400u
#define ___GFP_KSWAPD_RECLAIM	0x800u

#define __GFP_HARDWALL   ((__force gfp_t)___GFP_HARDWALL)
#define __GFP_THISNODE	((__force gfp_t)___GFP_THISNODE)
#define __GFP_HIGHMEM	((__force gfp_t)___GFP_HIGHMEM)
#define __GFP_NOWARN	((__force gfp_t)___GFP_NOWARN)
#define __GFP_NOMEMALLOC ((__force gfp_t)___GFP_NOMEMALLOC)

#define __GFP_DIRECT_RECLAIM	((__force gfp_t)___GFP_DIRECT_RECLAIM) /* Caller can reclaim */
#define __GFP_KSWAPD_RECLAIM	((__force gfp_t)___GFP_KSWAPD_RECLAIM) /* kswapd can wake */
#define __GFP_RECLAIM ((__force gfp_t)(___GFP_DIRECT_RECLAIM|___GFP_KSWAPD_RECLAIM))


static inline struct page *__alloc_pages(gfp_t gfp_mask, unsigned int order, int preferred_nid, nodemask_t *nodemask)
{
	return NULL;
}

static inline struct folio *__folio_alloc(gfp_t gfp, unsigned int order, int preferred_nid, nodemask_t *nodemask)
{
	return NULL;
}

#endif /* __LINUX_GFP_H */
