/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: SMAP MSG
 */

#ifndef _SMAP_MSG_H
#define _SMAP_MSG_H

#include <linux/workqueue.h>
#include <linux/time64.h>
#include <linux/completion.h>
#include "common.h"
extern unsigned int smap_pgsize;
extern unsigned int smap_mode;
extern unsigned int smap_scene;

#define DEFAULT_L1_NODE (0)
#define DEFAULT_L2_NODE (1)
#define INVALID_DATA_MODE (-1)

#define MN_HISTORY_SIZE 4

enum data_mode_args {
	ACTC_MODE = 0,
	CPI_MODE_AND = 1,
	CPI_MODE_SUM = 2,
	CPI_MODE_OR = 3,
	ACCESS_MODE_AND = 4,
	ACCESS_MODE_SUM = 5,
	ACCESS_MODE_OR = 6,
	MEBS_MODE = 7,
	MEBS_MODE_4B = 8,
	MEBS_MODE_6B = 9,
	PLDA_HDT_MODE = 10,
	PLDA_HDT_DECAY_MODE = 11,
	NR_DATA_MODE_ARGS
};

enum smap_pgsize_args {
	NORMAL_PAGE,
	HUGE_PAGE,
	NR_PGSIZE_ARGS,
};

enum smap_mode_args {
	BARE_MODE,
	VM_MODE,
	PROCESS_MODE,
	NR_MODE_ARGS,
};

enum smap_scene_args {
	NORMAL_SCENE,
	UB_QEMU_SCENE,
	UB_QEMU_SCENE_ADVANCED,
	NR_SCENE_ARGS,
};

enum page_type_stat {
	PAGE_TYPE_TRANSHUGE,
	PAGE_TYPE_HUGE,
	PAGE_TYPE_NOR_LRU,
	PAGE_TYPE_ZERO_REF,
	NR_ABNORMAL,
};

extern bool is_smap_pg_huge(void);

static inline bool is_bare_mode(void)
{
	return smap_mode == BARE_MODE;
}

static inline bool is_vm_mode(void)
{
	return smap_mode == VM_MODE;
}

static inline bool is_process_mode(void)
{
	return smap_mode == PROCESS_MODE;
}

static inline bool is_data_mode_invalid(int mode)
{
	return (unsigned long)mode >= NR_DATA_MODE_ARGS;
}

#endif /* _SMAP_MSG_H */