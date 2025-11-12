/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MMZONE_H
#define _LINUX_MMZONE_H

#ifndef __ASSEMBLY__
#ifndef __GENERATING_BOUNDS_H

#include <linux/list.h>
#include <linux/mm_types.h>
#include <linux/numa.h>
#include <linux/pageblock-flags.h>
#include <asm/mmzone.h>
#include <asm/page.h>

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
enum migratetype {
    MIGRATE_UNMOVABLE,
    MIGRATE_MOVABLE,
    MIGRATE_RECLAIMABLE,
    MIGRATE_PCPTYPES,
    MIGRATE_HIGHATOMIC = MIGRATE_PCPTYPES,
    MIGRATE_CMA,
    MIGRATE_ISOLATE,
    MIGRATE_TYPES
};
#define folio_migratetype(folio) -1
#define folio_test_hwpoison(folio) false

#endif /* LINUX_VERSION_CODE */

#define MIGRATETYPE_MASK ((1UL << PB_migratetype_bits) - 1)

enum zone_stat_item {
    NR_FREE_PAGES,
    NR_ZONE_LRU_BASE,
    NR_ZONE_INACTIVE_ANON = NR_ZONE_LRU_BASE,
    NR_ZONE_ACTIVE_ANON,
    NR_ZONE_INACTIVE_FILE,
    NR_ZONE_ACTIVE_FILE,
    NR_ZONE_UNEVICTABLE,
    NR_ZONE_WRITE_PENDING,
    NR_MLOCK,
    NR_BOUNCE,
#ifdef CONFIG_ZSMALLOC
    NR_ZSPAGES,
#endif
    NR_FREE_CMA_PAGES,
    NR_VM_ZONE_STAT_ITEMS
};

#define get_pageblock_migratetype(page)					\
    get_pfnblock_flags_mask(page, page_to_pfn(page), MIGRATETYPE_MASK)

enum zone_watermarks {
    WMARK_MIN = 0,
    WMARK_LOW = 1,
    WMARK_HIGH = 2,
    WMARK_PROMO = 3,
    NR_WMARK = 4
};

enum node_stat_item {
    NR_LRU_BASE,
    NR_INACTIVE_ANON = NR_LRU_BASE,
    NR_ACTIVE_ANON,
    NR_INACTIVE_FILE,
    NR_ACTIVE_FILE,
    NR_UNEVICTABLE,
    NR_ISOLATED_ANON,
    NR_ISOLATED_FILE,
};

#define MAX_NR_ZONES 8
#define MAX_ZONES_PER_ZONELIST (MAX_NUMNODES * MAX_NR_ZONES)

struct zone {
    int node;
    unsigned long		present_pages;
    unsigned long		spanned_pages;
    unsigned long		zone_start_pfn;
    const char *name;
    struct pglist_data *zone_pgdat;
};

enum {
    ZONELIST_FALLBACK,	/* zonelist with fallback */
    MAX_ZONELISTS
};

enum zone_type {
#ifdef CONFIG_ZONE_DMA
    ZONE_DMA,   /* ZONE_DMA */
#endif
#ifdef CONFIG_ZONE_DMA32
    ZONE_DMA32, /* ZONE_DMA32 */
#endif
    ZONE_NORMAL,
#ifdef CONFIG_HIGHMEM
    ZONE_HIGHMEM,
#endif
    ZONE_MOVABLE,
#ifdef CONFIG_ZONE_DEVICE
    ZONE_DEVICE,
#endif
    __MAX_NR_ZONES
};

struct zoneref {
    struct zone *zone;	/* Pointer to actual zone */
    int zone_idx;		/* zone_idx(zoneref->zone) */
};

struct zonelist {
    struct zoneref _zonerefs[MAX_ZONES_PER_ZONELIST + 1];
};

typedef struct pglist_data {
    struct zone node_zones[MAX_NR_ZONES];
    int node_id;
    unsigned long node_start_pfn;
    unsigned long node_present_pages;
    unsigned long node_spanned_pages; /* total size of physical page
                         range, including holes */
} pg_data_t;

extern struct pglist_data *node_data[];
#define NODE_DATA(nid)		(node_data[(nid)])

/* Returns true if a zone has memory */
static inline int populated_zone(struct zone *zone)
{
    return zone->present_pages;
}

#ifdef CONFIG_SPARSEMEM
#include <asm/sparsemem.h>
#endif

#define PFN_SECTION_SHIFT	(SECTION_SIZE_BITS - PAGE_SHIFT)
#define PAGES_PER_SECTION       (1UL << PFN_SECTION_SHIFT)

struct mem_section_usage {
    /* See declaration of similar field in struct zone */
    unsigned long pageblock_flags[0];
};

struct mem_section {
    struct mem_section_usage *usage;
};

static inline unsigned long *section_to_usemap(struct mem_section *ms)
{
    return ms->usage->pageblock_flags;
}

static inline struct mem_section *__pfn_to_section(unsigned long pfn)
{
    return NULL;
}

#define for_each_zone_zonelist_nodemask(zone, z, zlist, highidx, nodemask) \
    for ((z) = first_zones_zonelist(zlist, highidx, nodemask), (zone) = zonelist_zone(z);	\
        zone;							\
        (z) = next_zones_zonelist(++(z), highidx, nodemask),	\
            (zone) = zonelist_zone(z))

static inline int zonelist_zone_idx(struct zoneref *zoneref)
{
    return zoneref->zone_idx;
}

static inline int zone_to_nid(struct zone *zone)
{
    return -1;
}

static inline int zonelist_node_idx(struct zoneref *zoneref)
{
    return zone_to_nid(zoneref->zone);
}

/*
 * zone_idx() returns 0 for the ZONE_DMA zone, 1 for the ZONE_NORMAL zone, etc.
 */
#define zone_idx(zone)		((zone) - (zone)->zone_pgdat->node_zones)

static inline int is_highmem_idx(enum zone_type idx)
{
    return 0;
}

#endif /* !__GENERATING_BOUNDS.H */
#endif /* !__ASSEMBLY__ */
#endif /* _LINUX_MMZONE_H */
