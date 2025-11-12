/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _LINUX_RWSEM_H
#define _LINUX_RWSEM_H

#include <linux/spinlock.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rw_semaphore {
    int count;
};

#define __RWSEM_INITIALIZER(name) { 0 }

int down_read_killable(struct rw_semaphore *sem);
void down_read(struct rw_semaphore *sem);
void up_read(struct rw_semaphore *sem);
extern void down_write(struct rw_semaphore *sem);
extern void up_write(struct rw_semaphore *sem);

#define init_rwsem(data) data

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_RWSEM_H */
