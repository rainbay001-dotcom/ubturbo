/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LINUX_SEMAPHORE_H
#define __LINUX_SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

struct semaphore {
    unsigned int count;
};

#define __SEMAPHORE_INITIALIZER(name, n)                                \
{                                                                       \
    .count = (n),                                                         \
}

#define DEFINE_SEMAPHORE(_name, _n)     \
    struct semaphore (_name) = __SEMAPHORE_INITIALIZER(_name, _n)

static inline void sema_init(struct semaphore *sem, int val)
{
    sem->count = val;
}

extern void up(struct semaphore *sem);
extern void down(struct semaphore *sem);
extern int down_interruptible(struct semaphore *sem);

#ifdef __cplusplus
}
#endif
#endif /* __LINUX_SEMAPHORE_H */
