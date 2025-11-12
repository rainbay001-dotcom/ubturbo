/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MIGRATE_H
#define _LINUX_MIGRATE_H

#include <linux/mm_types.h>
#include <linux/migrate_mode.h>
#include <linux/mempolicy.h>
#include <linux/hugetlb.h>
#include <linux/version.h>

#ifdef __cplusplus
extern "C" {
#endif

struct folio *alloc_migration_target(struct folio *src, unsigned long private_);
typedef struct folio *new_folio_t(struct folio *folio, unsigned long private_);
typedef void free_folio_t(struct folio *folio, unsigned long private_);
extern int isolate_and_migrate_folios(struct folio **folios, unsigned int nr_folios,
        new_folio_t get_new_folio, free_folio_t put_new_folio,
        unsigned long private_, enum migrate_mode mode,
        unsigned int *nr_succeeded);

extern void putback_movable_pages(struct list_head *l);
void shake_page(struct page *p);
#ifdef __cplusplus
}
#endif

#endif /* _LINUX_MIGRATE_H */
