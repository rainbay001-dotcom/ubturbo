/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef HAM_TASKS_MGR_H
#define HAM_TASKS_MGR_H

#include <linux/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_INIT_PID (-1)
#define HAM_MAX_TASK 32

#define HAM_TASK_IDLE 0x0
#define HAM_TASK_INITED 0x1
#define HAM_TASK_MIGRATED 0x2
#define HAM_TASK_STOPPED 0x4
#define HAM_TASK_ALLOW_MIGR 0x40000000
#define HAM_TASK_OCCUPIED 0x80000000

struct ham_page_map {
	struct list_head list;
	struct folio *src_folio;
	struct folio *dst_folio;
	int src_numa_id;
	bool present;
	bool is_migrate;
	unsigned short freq;
};

struct ram_block_map {
	unsigned long rmt_numa_start;
	unsigned long size;
	unsigned long hva_start;
	struct ham_page_map *hpms;
	unsigned int page_num;
	int rmt_numa_id;
	bool cacheable;
};

struct ham_migrate_task {
	unsigned int status;
	bool is_finish;
	unsigned long finish_times;
	pid_t pid;
	unsigned int numa_cnt;
	struct ram_block_map *
		ram_maps; /* the mapping relationship between local pages and remote pages */
	struct hstate *hstate;
};

void init_task_list(void);

struct ham_migrate_task *allocate_migrate_task(pid_t pid);

struct ham_migrate_task *get_migrate_task(pid_t pid);

void put_migrate_task(struct ham_migrate_task *mig_task, unsigned int status);

void release_migrate_task(struct ham_migrate_task *mig_task);

void release_finished_tasks(void);

void release_all_tasks(void);

int check_task_status(struct ham_migrate_task *mig_task,
		      unsigned int expected_status);

#ifdef __cplusplus
}
#endif
#endif
