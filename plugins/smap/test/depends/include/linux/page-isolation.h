/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_PAGEISOLATION_H
#define __LINUX_PAGEISOLATION_H


#ifdef __cplusplus
extern "C" {
#endif
 
#define MEMORY_OFFLINE	0x1
#define REPORT_FAILURE	0x2

#define MEMORY_OFFLINE	0x1
#define REPORT_FAILURE	0x2

// Check if the given migration type is an isolate type
// Returns true if it is an isolate type, false otherwise
static inline bool is_migrate_isolate(int migratetype)
{
    return false;
}

// Check if the given page belongs to the isolate migration type
// Returns true if the page belongs to the isolate type, false otherwise
static inline bool is_migrate_isolate_page(struct page *page)
{
    return false;
}

// Check if the given memory zone contains isolated page blocks
// Returns true if it contains isolated page blocks, false otherwise
static inline bool has_isolate_pageblock(struct zone *zone)
{
    return false;
}

#ifdef __cplusplus
}
#endif

#endif
