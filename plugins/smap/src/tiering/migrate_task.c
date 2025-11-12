// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap migrate task module
 */
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include "migrate_back_debugfs.h"
#include "migrate_task.h"

#define MAX_MIGRATE_BACK_TASK_COUNT 100

LIST_HEAD(migrate_back_task_list);
DEFINE_SPINLOCK(migrate_back_task_lock);

struct migrate_back_task *init_migrate_back_task(unsigned long long task_id)
{
	struct migrate_back_task *task;

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		return NULL;
	task->task_id = task_id;
	INIT_LIST_HEAD(&task->subtask);
	return task;
}

void free_all_migrate_back_task(void)
{
	struct migrate_back_subtask *s, *stmp;
	struct migrate_back_task *t, *ttmp;

	spin_lock(&migrate_back_task_lock);
	list_for_each_entry_safe(t, ttmp, &migrate_back_task_list, task_node) {
		list_for_each_entry_safe(s, stmp, &t->subtask, task_list) {
			list_del(&s->task_list);
			kfree(s);
		}
		kfree(t);
	}
	spin_unlock(&migrate_back_task_lock);
}

int init_migrate_back_subtask(struct migrate_back_task *task,
			      struct migrate_back_inner_payload *p,
			      struct migrate_back_subtask **result)
{
	struct migrate_back_subtask *subtask;

	subtask = kzalloc(sizeof(*subtask), GFP_KERNEL);
	if (!subtask) {
		return -ENOMEM;
	}

	subtask->src_nid = p->src_nid;
	subtask->dest_nid = p->dest_nid;
	subtask->pa_start = p->pa_start;
	subtask->pa_end = p->pa_end;
	subtask->task = task;
	subtask->isolated_flag = true;
	*result = subtask;

	return 0;
}

void clear_migrate_back_task(void)
{
	int count = 0;
	struct migrate_back_subtask *s, *stmp;
	struct migrate_back_task *t, *ttmp;
	spin_lock(&migrate_back_task_lock);
	list_for_each_entry_safe(t, ttmp, &migrate_back_task_list, task_node) {
		count++;
		if (count > MAX_MIGRATE_BACK_TASK_COUNT &&
		    t->status > MB_TASK_WAITING) {
			list_for_each_entry_safe(s, stmp, &t->subtask,
						 task_list) {
				list_del(&s->task_list);
				kfree(s);
			}
			remove_migrate_back_debugfs(t);
			list_del(&t->task_node);
			kfree(t);
		}
	}
	spin_unlock(&migrate_back_task_lock);
}