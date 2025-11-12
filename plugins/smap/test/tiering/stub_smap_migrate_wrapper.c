/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: SMAP migrate wrapper stub file
 */
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/migrate.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>

int in_interrupt(void)
{
	return 0;
}

bool is_migrate_cma_page(struct page *page)
{
	return false;
}

int PageHWPoison(struct page *page)
{
	return 0;
}

void ClearHPageFreed(struct page *page)
{
}

void lockdep_assert_held(struct mutex l)
{
}

struct zonelist *node_zonelist(int nid, gfp_t flags)
{
	return NULL;
}

unsigned int read_mems_allowed_begin(void)
{
	return 0;
}
enum zone_type gfp_zone(gfp_t flags)
{
	return 0;
}

struct zoneref *next_zones_zonelist(struct zoneref *z,
					enum zone_type highest_zoneidx,
					nodemask_t *nodes)
{
	return z;
}

struct zoneref *first_zones_zonelist(struct zonelist *zonelist,
					enum zone_type highest_zoneidx,
					nodemask_t *nodes)
{
	return NULL;
}

struct zone *zonelist_zone(struct zoneref *zoneref)
{
	return NULL;
}

bool cpuset_zone_allowed(struct zone *z, gfp_t gfp_mask)
{
	return true;
}

bool read_mems_allowed_retry(unsigned int seq)
{
	return false;
}

gfp_t htlb_alloc_mask(struct hstate *h)
{
	return 0;
}

bool tsk_is_oom_victim(struct task_struct * tsk)
{
	return tsk->signal->oom_mm;
}
void folio_ref_inc(struct folio *folio)
{
}
void folio_clear_hugetlb_freed(struct folio *folio)
{
}