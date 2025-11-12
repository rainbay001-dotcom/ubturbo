/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

#include <linux/list.h>
#include <linux/pid.h>
#include <linux/kprobes.h>
#include <linux/nodemask.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct signal_struct;

#define PF_EXITING         0x00000004      /* Getting shut down */
#define PF_MEMALLOC_PIN		0x10000000	/* Allocation context constrained to zones which allow long term pinning. */

#define PF_MEMALLOC_NOCMA	0x10000000

enum {
	TASK_COMM_LEN = 16,
};

struct task_struct {
	unsigned int			flags;
	struct list_head		tasks;
	pid_t				pid;
	struct mm_struct *mm;
	char				comm[TASK_COMM_LEN];
	struct signal_struct *signal;
	/* Protected by ->alloc_lock: */
	nodemask_t			mems_allowed;
	int				nr_cpus_allowed;
	struct files_struct		*files;
    pid_t				tgid;
};

void schedule(void);
extern char *__get_task_comm(char *to, size_t len, struct task_struct *tsk);
extern struct task_struct *find_get_task_by_vpid(pid_t nr);
#define get_task_comm(buf, tsk) ({			\
	__get_task_comm(buf, sizeof(buf), tsk);		\
})

#define cond_resched() ({			\
				\
})

#ifdef __cplusplus
}
#endif
#endif
