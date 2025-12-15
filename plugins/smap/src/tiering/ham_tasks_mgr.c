/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rwlock.h>
#include <linux/rwlock_types.h>
#include <linux/hugetlb.h>
#include <linux/jiffies.h>

#include "ham_tasks_mgr.h"

#undef pr_fmt
#define pr_fmt(fmt) "HAM: " fmt
static rwlock_t g_tasklock;
static struct ham_migrate_task g_tasklist[HAM_MAX_TASK];

static void reset_migrate_task(struct ham_migrate_task *mig_task)
{
	unsigned int i;
	mig_task->is_finish = false;
	mig_task->pid = TASK_INIT_PID;
	mig_task->hstate = NULL;
	if (mig_task->ram_maps) {
		for (i = 0; i < mig_task->numa_cnt; i++) {
			if (!mig_task->ram_maps[i].hpms) {
				continue;
			}
			vfree(mig_task->ram_maps[i].hpms);
			mig_task->ram_maps[i].hpms = NULL;
		}
		kfree(mig_task->ram_maps);
	}
	mig_task->ram_maps = NULL;
	mig_task->numa_cnt = 0;
	mig_task->status = HAM_TASK_IDLE;
}

static void release_migrate_task_inner(struct ham_migrate_task *mig_task)
{
	unsigned int i, j;
	struct ham_page_map *hpm;

	if (!mig_task->ram_maps) {
		goto free_buff;
	}
	for (i = 0; i < mig_task->numa_cnt; i++) {
		if (!mig_task->ram_maps[i].hpms) {
			continue;
		}
		for (j = 0; j < mig_task->ram_maps[i].page_num; j++) {
			hpm = &mig_task->ram_maps[i].hpms[j];
			if (hpm_test_migrate(hpm) || hpm_test_rollback(hpm)) {
				continue;
			}
			if (!hpm->dst_folio) {
				continue;
			}
			putback_hugetlb_folio(hpm->dst_folio);
		}
	}
free_buff:
	reset_migrate_task(mig_task);
}

void release_migrate_task(struct ham_migrate_task *mig_task)
{
	write_lock(&g_tasklock);
	release_migrate_task_inner(mig_task);
	write_unlock(&g_tasklock);
}

struct ham_migrate_task *get_migrate_task(pid_t pid)
{
	int i;
	struct ham_migrate_task *mig_task;
	write_lock(&g_tasklock);
	for (i = 0; i < HAM_MAX_TASK; i++) {
		if (g_tasklist[i].pid == pid) {
			mig_task = &g_tasklist[i];
			if (mig_task->status & HAM_TASK_OCCUPIED) {
				pr_err("current task is occupied, pid: %d\n",
				       pid);
				goto out;
			}
			mig_task->status |= HAM_TASK_OCCUPIED;
			write_unlock(&g_tasklock);
			return mig_task;
		}
	}
out:
	write_unlock(&g_tasklock);
	return NULL;
}

void put_migrate_task(struct ham_migrate_task *mig_task, unsigned int status)
{
	mig_task->status |= status;
	mig_task->status &= ~HAM_TASK_OCCUPIED;
}

struct ham_migrate_task *allocate_migrate_task(pid_t pid)
{
	int i;
	struct ham_migrate_task *mig_task;

	write_lock(&g_tasklock);
	for (i = 0; i < HAM_MAX_TASK; i++) {
		if (g_tasklist[i].pid == pid) {
			pr_err("task already exists of pid: %d.\n", pid);
			write_unlock(&g_tasklock);
			return NULL;
		}
	}

	for (i = 0; i < HAM_MAX_TASK; i++) {
		if (g_tasklist[i].status != HAM_TASK_IDLE) {
			continue;
		}
		g_tasklist[i].pid = pid;
		g_tasklist[i].status = HAM_TASK_OCCUPIED;
		g_tasklist[i].is_finish = false;
		mig_task = &g_tasklist[i];
		write_unlock(&g_tasklock);
		return mig_task;
	}
	write_unlock(&g_tasklock);
	pr_err("HAM migrate task is full\n");
	return NULL;
}

void release_finished_tasks(void)
{
	int i;
	write_lock(&g_tasklock);
	for (i = 0; i < HAM_MAX_TASK; i++) {
		if (g_tasklist[i].status == HAM_TASK_IDLE) {
			continue;
		}
		if ((g_tasklist[i].pid != TASK_INIT_PID) &&
		    (!find_vpid(g_tasklist[i].pid))) {
			pr_info("the source VM does not exist, resources are going to release\n");
			release_migrate_task_inner(&g_tasklist[i]);
			continue;
		}
		if (g_tasklist[i].is_finish &&
		    (g_tasklist[i].finish_times <= jiffies)) {
			pr_info("timeup, resources are going to release\n");
			release_migrate_task_inner(&g_tasklist[i]);
			continue;
		}
	}
	write_unlock(&g_tasklock);
}

void release_all_tasks(void)
{
	int i;
	for (i = 0; i < HAM_MAX_TASK; i++) {
		release_migrate_task_inner(&g_tasklist[i]);
	}
}

void init_task_list(void)
{
	int i;
	for (i = 0; i < HAM_MAX_TASK; i++) {
		g_tasklist[i].is_finish = false;
		g_tasklist[i].pid = TASK_INIT_PID;
		g_tasklist[i].numa_cnt = 0;
		g_tasklist[i].ram_maps = NULL;
		g_tasklist[i].status = HAM_TASK_IDLE;
	}
	rwlock_init(&g_tasklock);
}

int check_task_status(struct ham_migrate_task *mig_task,
		      unsigned int expected_status)
{
	if (!(mig_task->status & expected_status)) {
		pr_err("check task status error, current task status is %#x, but expected status is %#x\n",
		       mig_task->status, expected_status);
		return -EINVAL;
	}
	return 0;
}
