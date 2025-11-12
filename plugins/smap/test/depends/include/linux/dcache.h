/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_DCACHE_H
#define __LINUX_DCACHE_H

struct qstr {
	const char *name;
};

struct dentry {
    struct qstr d_name;
    struct dentry *d_parent;
};

#endif	/* __LINUX_DCACHE_H */
