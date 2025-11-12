/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/fs.h>

struct file *filp_open(const char *filename, int flags, umode_t mode)
{
	return NULL;
}

int filp_close(struct file *filp, fl_owner_t id)
{
	return 0;
}
