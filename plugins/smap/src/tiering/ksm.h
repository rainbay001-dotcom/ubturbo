/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP KSM module
 */

#ifndef _SRC_TIERING_KSM_H
#define _SRC_TIERING_KSM_H

#include <linux/mm_types.h>
#include <linux/rmap.h>
#include <linux/interval_tree_generic.h>

struct ksm_stable_node {
	union {
		struct rb_node r_node;
		struct {
			struct list_head *head;
			struct {
				struct hlist_node hlist_dup;
				struct list_head list;
			};
		};
	};
	struct hlist_head hlist;
	union {
		unsigned long kpfn;
		unsigned long chain_prune_time;
	};
	/*
	 * STABLE_NODE_CHAIN can be any negative number in
	 * rmap_hlist_len negative range, but better not -1 to be able
	 * to reliably detect underflows.
	 */
#define STABLE_NODE_CHAIN (-1024)
	int rmap_hlist_len;
#ifdef CONFIG_NUMA
	int nid;
#endif
};

struct ksm_rmap_item {
	struct ksm_rmap_item *rmap_list;
	union {
		struct anon_vma *anon_vma;
#ifdef CONFIG_NUMA
		int nid;
#endif
	};
	struct mm_struct *mm;
	unsigned long address;
	unsigned int oldchecksum;
	union {
		struct rb_node node;
		struct {
			struct ksm_stable_node *head;
			struct hlist_node hlist;
		};
	};
};

struct vm_area_struct *smap_vma_interval_tree_iter_first(
	struct rb_root_cached *root, unsigned long start, unsigned long last);
struct vm_area_struct *smap_vma_interval_tree_iter_next(
	struct vm_area_struct *node, unsigned long start, unsigned long last);

struct anon_vma_chain *smap_anon_vma_interval_tree_iter_first(
	struct rb_root_cached *root, unsigned long start, unsigned long last);
struct anon_vma_chain *smap_anon_vma_interval_tree_iter_next(
	struct anon_vma_chain *node, unsigned long start, unsigned long last);

#define smap_anon_vma_interval_tree_foreach(avc, root, start, last)      \
	for ((avc) = smap_anon_vma_interval_tree_iter_first(root, start, \
							    last);       \
	     (avc); (avc) = smap_anon_vma_interval_tree_iter_next(       \
			    (avc), start, last))

#define smap_vma_interval_tree_foreach(vma, root, start, last)             \
	for ((vma) = smap_vma_interval_tree_iter_first(root, start, last); \
	     (vma);                                                        \
	     (vma) = smap_vma_interval_tree_iter_next((vma), start, last))

void smap_rmap_walk_ksm(struct folio *folio, struct rmap_walk_control *rwc);

#endif /* _SRC_TIERING_KSM_H */
