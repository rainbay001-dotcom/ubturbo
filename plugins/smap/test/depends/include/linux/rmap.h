/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_RMAP_H
#define _LINUX_RMAP_H

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/memcontrol.h>
#include <linux/version.h>
#include <linux/rwlock.h>

#ifdef __cplusplus
extern "C" {
#endif

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
int folio_referenced(struct folio *folio, int is_locked,
		     struct mem_cgroup *memcg, unsigned long *vm_flags);
#else
int page_referenced(struct page *page, int is_locked, struct mem_cgroup *memcg,
		    unsigned long *vm_flags);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _LINUX_RMAP_H */
