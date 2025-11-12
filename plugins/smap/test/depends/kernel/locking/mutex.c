/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/mutex.h>

void mutex_lock(struct mutex *lock)
{
}

int mutex_trylock(struct mutex *lock)
{
    return 1;
}

void mutex_unlock(struct mutex *lock)
{
}