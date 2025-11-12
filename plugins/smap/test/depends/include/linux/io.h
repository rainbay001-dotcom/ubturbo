/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2006 PathScale, Inc.  All Rights Reserved.
 */

#ifndef _LINUX_IO_H
#define _LINUX_IO_H

#define _IOC_DIRBITS	3
#define _IOC_SIZEBITS	13
#define _IOC_TYPEBITS	8
#define _IOC_NRBITS	8

#define _IOC_WRITE	4U
#define _IOC_READ	2U
#define _IOC_NONE	1U

#define _IOC_NRMASK	((1 << _IOC_NRBITS) - 1)
#define _IOC_TYPEMASK	((1 << _IOC_TYPEBITS) - 1)
#define _IOC_SIZEMASK	((1 << _IOC_SIZEBITS) - 1)
#define _IOC_DIRMASK	((1 << _IOC_DIRBITS) - 1)

#define _IOC_NRSHIFT 0
#define _IOC_TYPESHIFT (_IOC_NRBITS + _IOC_NRSHIFT)
#define _IOC_SIZESHIFT (_IOC_TYPEBITS + _IOC_TYPESHIFT)
#define _IOC_DIRSHIFT (_IOC_SIZEBITS + _IOC_SIZESHIFT)

#define _IOC(direction, type, number, size)			\
	((unsigned int)				\
	 (((direction) << _IOC_DIRSHIFT) |	((type) << _IOC_TYPESHIFT) |	\
	  ((number) << _IOC_NRSHIFT) | ((size) << _IOC_SIZESHIFT)))

#define _IOW(type, number, size) _IOC(_IOC_WRITE, (type), (number), sizeof(size))
#define _IOR(type, number, size) _IOC(_IOC_READ, (type), (number), sizeof(size))

void memunmap(void *addr);
void *memremap(resource_size_t offset, size_t size, unsigned long flags);

#endif