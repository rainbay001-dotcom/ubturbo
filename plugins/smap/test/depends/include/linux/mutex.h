/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef TEST_MUTEX_H
#define TEST_MUTEX_H

struct mutex {
    int		owner;
};

#define mutex_init(mutex)	0

#define __MUTEX_INITIALIZER(lockname) \
		{ .owner = 0 }

#define DEFINE_MUTEX(mutexname) \
	struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

void mutex_lock(struct mutex *lock);

int mutex_trylock(struct mutex *lock);

void mutex_unlock(struct mutex *lock);

#endif // TEST_MUTEX_H
