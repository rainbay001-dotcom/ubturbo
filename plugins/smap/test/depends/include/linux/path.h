/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __PATH_H
#define __PATH_H

#include <linux/dcache.h>

struct path {
	struct dentry *dentry;
};

#endif
