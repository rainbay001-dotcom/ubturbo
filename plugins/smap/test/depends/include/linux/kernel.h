/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_KERNEL_DEPENDS_H
#define _LINUX_KERNEL_DEPENDS_H

#include <linux/printk.h>
#include <linux/build_bug.h>
#include <linux/stdarg.h>
#include <uapi/linux/kernel.h>
#include <linux/bitops.h>
#include <linux/kstrtox.h>
#include <linux/atomic.h>
#include <linux/kconfig.h>
#include <linux/minmax.h>
#include <linux/capability.h>

#define CAP_SYS_ADMIN        21

#define ALIGN(x, a)		__ALIGN_KERNEL((x), (a))

#define DIV_ROUND_UP __KERNEL_DIV_ROUND_UP
#define READ			0
#define WRITE			1

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define VERIFY_OCTAL_PERMISSIONS(permissions)						\
	(BUILD_BUG_ON_ZERO((permissions) < 0) +					\
	 BUILD_BUG_ON_ZERO((permissions) > 0777) +					\
	 /* USER_READABLE >= GROUP_READABLE >= OTHER_READABLE */		\
	 BUILD_BUG_ON_ZERO((((permissions) >> 6) & 4) < (((permissions) >> 3) & 4)) +	\
	 BUILD_BUG_ON_ZERO((((permissions) >> 3) & 4) < ((permissions) & 4)) +		\
	 /* USER_WRITABLE >= GROUP_WRITABLE */					\
	 BUILD_BUG_ON_ZERO((((permissions) >> 6) & 2) < (((permissions) >> 3) & 2)) +	\
	 /* OTHER_WRITABLE?  Generally considered a bad idea. */		\
	 BUILD_BUG_ON_ZERO((permissions) & 2) +					\
	 (permissions))

int scnprintf(char *buf, size_t size, const char *fmt, ...);

#endif
