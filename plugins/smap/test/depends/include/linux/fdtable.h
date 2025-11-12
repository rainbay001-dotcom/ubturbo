/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __FD_TABLE_H
#define __FD_TABLE_H

#include <linux/fs.h>
#include <linux/spinlock.h>

struct fdtable {
	unsigned int max_fds;
	struct file **fd;      /* current fd array */
	unsigned long *close_on_exec;
	unsigned long *open_fds;
	unsigned long *full_fds_bits;
};

struct files_struct {
	struct fdtable *fdt;
	spinlock_t file_lock;
};


#ifdef __cplusplus
extern "C" {
#endif

struct fdtable *files_fdtable(struct files_struct *files);

#ifdef __cplusplus
}
#endif

#endif
