/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP rmap module
 */

#ifndef _SRC_TIERING_RMAP_H
#define _SRC_TIERING_RMAP_H

#include <linux/mm_types.h>

enum page_task_type {
	PAGE_PID_TYPE,
	PAGE_NODE_TYPE,
};

struct page_task_arg {
	enum page_task_type type;
	pid_t pid;
	bool found;
	int node;
	int nr_cpus_allowed;
};

extern void unlock_page(struct page *page);

void find_page_task(struct page *page, int is_locked,
		    struct page_task_arg *pta);

#endif /* _SRC_TIERING_RMAP_H */
