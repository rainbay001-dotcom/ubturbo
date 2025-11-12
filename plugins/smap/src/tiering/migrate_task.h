/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap migrate task module
 */

#ifndef _SRC_TIERING_MIGRATE_TASK_H
#define _SRC_TIERING_MIGRATE_TASK_H

enum migrate_back_task_status {
	MB_TASK_CREATED,
	MB_TASK_WAITING,
	MB_TASK_DONE,
	MB_TASK_ERR,
};

struct migrate_back_task {
	unsigned long long task_id;
	int subtask_cnt;
	int progress;
	enum migrate_back_task_status status;
	struct list_head task_node;
	struct list_head subtask;
};
extern struct list_head migrate_back_task_list;
extern spinlock_t migrate_back_task_lock;

enum mb_subtask_status {
	MB_SUBTASK_WAITING,
	MB_SUBTASK_DONE,
	MB_SUBTASK_ERR,
};

struct migrate_back_subtask {
	bool isolated_flag;
	int src_nid;
	int dest_nid;
	struct migrate_back_task *task;
	enum mb_subtask_status status;
	__u64 pa_start;
	__u64 pa_end;
	struct list_head task_list;
};

struct migrate_back_inner_payload {
	int src_nid;
	int dest_nid;
	u64 pa_start;
	u64 pa_end;
};

struct migrate_back_task *init_migrate_back_task(unsigned long long task_id);
void free_all_migrate_back_task(void);
void clear_migrate_back_task(void);

int init_migrate_back_subtask(struct migrate_back_task *task,
			      struct migrate_back_inner_payload *p,
			      struct migrate_back_subtask **result);

#endif /* _SRC_TIERING_MIGRATE_TASK_H */
