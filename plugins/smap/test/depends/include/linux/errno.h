/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_ERRNO_DEPENDS_H
#define _LINUX_ERRNO_DEPENDS_H

#include <uapi/linux/errno.h>

#define ENOPARAM	519	/* ENOPARAM: Parameter not supported */
#define EOPENSTALE	518	/* EOPENSTALE: open found a stale dentry */
#define EPROBE_DEFER	517	/* EPROBE_DEFER: Driver requests probe retry */
#define ERESTART_RESTARTBLOCK 516 /* ERESTART_RESTARTBLOCK: restart by calling sys_restart_syscall */
#define ENOIOCTLCMD	515	/* ENOIOCTLCMD: No ioctl command */
#define ERESTARTNOHAND	514	/* ERESTARTNOHAND: restart if no handler.. */
#define ERESTARTNOINTR	513 /* ERESTARTNOINTR */
#define ERESTARTSYS	512 /* ERESTARTSYS */

#define ENOGRACE	531	/* ENOGRACE: NFS file lock reclaim refused */
#define ERECALLCONFLICT	530	/* ERECALLCONFLICT: conflict with recalled state */
#define EIOCBQUEUED	529	/* EIOCBQUEUED: iocb queued, will get completion event */
#define EJUKEBOX	528	/* EJUKEBOX: Request initiated, but will not complete before timeout */
#define EBADTYPE	527	/* EBADTYPE: Type not supported by server */
#define ESERVERFAULT	526	/* ESERVERFAULT: An untranslatable error occurred */
#define ETOOSMALL	525	/* ETOOSMALL: Buffer or request is too small */
#define ENOTSUPP	524	/* ENOTSUPP: Operation is not supported */
#define EBADCOOKIE	523	/* EBADCOOKIE: Cookie is stale */
#define ENOTSYNC	522	/* ENOTSYNC: Update synchronization mismatch */
#define EBADHANDLE	521	/* EBADHANDLE: Illegal NFS file handle */

#endif
