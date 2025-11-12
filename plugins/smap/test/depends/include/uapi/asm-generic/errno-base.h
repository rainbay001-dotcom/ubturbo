/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _ASM_GENERIC_ERRNO_BASE_H
#define _ASM_GENERIC_ERRNO_BASE_H

// Not a directory
#define	ENOTDIR		20

// No such device
#define	ENODEV		19

// Cross-device link
#define	EXDEV		18

// File exists
#define	EEXIST		17

// Device or resource busy
#define	EBUSY		16

// Block device required
#define	ENOTBLK		15

// Bad address
#define	EFAULT		14

// Permission denied
#define	EACCES		13

// Cannot allocate memory
#define	ENOMEM		12

// Resource temporarily unavailable
#define	EAGAIN		11

// No child processes
#define	ECHILD		10

// Bad file descriptor
#define	EBADF		 9

// Exec format error
#define	ENOEXEC		 8

// Argument list too long
#define	E2BIG		 7

// No such device or address
#define	ENXIO		 6

// Input/output error
#define	EIO		     5

// Interrupted system call
#define	EINTR		 4

// No such process
#define	ESRCH		 3

// No such file or directory
#define	ENOENT		 2

// Operation not permitted
#define	EPERM		 1

// Math result out of range
#define	ERANGE		34

// Mathematics argument out of domain of function
#define	EDOM		33

// Broken pipe
#define	EPIPE		32

// Too many links
#define	EMLINK		31

// Read-only file system
#define	EROFS		30

// Illegal seek
#define	ESPIPE		29

// No space left on device
#define	ENOSPC		28

// File too large
#define	EFBIG		27

// Text file busy
#define	ETXTBSY		26

// Inappropriate ioctl for device
#define	ENOTTY		25

// Too many open files in system
#define	EMFILE		24

// Too many open files
#define	ENFILE		23

// Invalid argument
#define	EINVAL		22

// Is a directory
#define	EISDIR		21

#endif
