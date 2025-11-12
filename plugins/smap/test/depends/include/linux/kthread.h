/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_KTHREAD_H
#define _LINUX_KTHREAD_H

#include <linux/err.h>
#include <linux/sched.h>
#ifdef __cplusplus
extern "C" {
#endif
struct task_struct *kthread_create_on_node(int (*threadfn)(void *data),
					   void *data,
					   int node,
					   const char namefmt[], ...);

#define kthread_create(thread_func, data, name_format, arg...) \
	kthread_create_on_node(thread_func, data, NUMA_NO_NODE, name_format, ##arg)

#define kthread_run(thread_func, data, name_format, ...)			   \
({									   \
	struct task_struct *__depends_k						   \
		= kthread_create(thread_func, data, name_format, ## __VA_ARGS__); \
	if (!IS_ERR(__depends_k))						   \
		wake_up_process(__depends_k);					   \
	__depends_k;								   \
})

int kthread_stop(struct task_struct *k);

bool kthread_should_stop(void);
#ifdef __cplusplus
}
#endif
#endif /* _LINUX_KTHREAD_H */