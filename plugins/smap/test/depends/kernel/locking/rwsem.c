/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/rwsem.h>

int down_read_killable(struct rw_semaphore *sem)
{
	return 0;
}

void down_read(struct rw_semaphore *sem)
{
}

void up_read(struct rw_semaphore *sem)
{
}

void down_write(struct rw_semaphore *sem)
{
}

void up_write(struct rw_semaphore *sem)
{
}