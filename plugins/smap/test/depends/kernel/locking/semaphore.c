/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/semaphore.h>

void up(struct semaphore *sem)
{
    sem->count++;
}

void down(struct semaphore *sem)
{
    sem->count--;
}

int down_interruptible(struct semaphore *sem)
{
    sem->count--;
    return 0;
}