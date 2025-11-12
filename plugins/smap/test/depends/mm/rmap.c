/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/memcontrol.h>
#include <linux/rmap.h>
#include <linux/mm_types.h>

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
int folio_referenced(struct folio *folio, int is_locked, struct mem_cgroup *memcg, unsigned long *vm_flags)
{
    return 0;
}
#else
int page_referenced(struct page *page, int is_locked, struct mem_cgroup *memcg, unsigned long *vm_flags)
{
    return 0;
}
#endif