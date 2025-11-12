/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef __LINUX_IOCTL_H
#define __LINUX_IOCTL_H

#define _IOC_NRBITS	8
#define _IOC_TYPEBITS	8

#ifndef _IOC_SIZEBITS
# define _IOC_SIZEBITS	14
#endif

#ifndef _IOC_WRITE
# define _IOC_WRITE	1U
#endif

#ifndef _IOC_NONE
# define _IOC_NONE	0U
#endif

#define _IOC_NRMASK	((1 << _IOC_NRBITS)-1)
#define _IOC_TYPEMASK	((1 << _IOC_TYPEBITS)-1)
#define _IOC_NRSHIFT	0
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)

#define _IOC(direction, type, nr, size) \
	(((direction)  << _IOC_DIRSHIFT) | ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | ((size) << _IOC_SIZESHIFT))

#define _IOC_TYPECHECK(t) (sizeof(t))
#define _IOW(type, nr, size)	_IOC(_IOC_WRITE, (type), (nr), (_IOC_TYPECHECK(size)))
#define _IO(type, nr)		_IOC(_IOC_NONE, (type), (nr), 0)
#define _IOC_NR(nr)		(((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_TYPE(nr)		(((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)

#endif
