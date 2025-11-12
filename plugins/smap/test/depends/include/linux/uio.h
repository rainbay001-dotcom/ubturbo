/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LINUX_UIO_H
#define __LINUX_UIO_H

struct kvec {
	void *iov_base; /* and that should *never* hold a userland pointer */
	size_t iov_len;
};

struct iov_iter {
	const struct kvec *kvec;
	unsigned long nr_segs;
};

#endif /* __LINUX_UIO_H */
