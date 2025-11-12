/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: SMAP MIGRATE PAGES
 */

#ifndef SMAP_MIGRATE_PAGES_H
#define SMAP_MIGRATE_PAGES_H

#include "smap_msg.h"
#include "migrate_task.h"

#define MAX_NR_MIGRATE_THREADS 32
#define MAX_MIG_LIST_NR (1 << 28)

extern int nr_local_numa;
extern int isolate_lru_page(struct page *page);
extern ktime_t calc_time_us(ktime_t start_time);

/* 分配2M页面 */
struct folio *smap_alloc_new_node_page(struct folio *folio, unsigned long node);
struct folio *smap_alloc_new_node_page_mig_back(struct folio *folio,
						unsigned long node);
unsigned int smap_migrate_numa(struct migrate_numa_inner_msg *msg);
unsigned int smap_migrate(struct folio **folios, unsigned int nr_folios,
			  int to_node, bool is_mig_back);
struct folio *alloc_demote_page(struct folio *folio, unsigned long node);

bool is_folio_in_migrate_back_range(struct folio *folio);
struct folio *smap_alloc_huge_page_node(struct folio *folio, int nid,
					bool is_mig_back);

void smap_handle_migrate_back_subtask(struct migrate_back_subtask *task);
void smap_handle_migrate_back_subtask_4k(struct migrate_back_subtask *task);
int do_migrate(struct migrate_msg *msg, struct mig_list *mig_list);
#endif