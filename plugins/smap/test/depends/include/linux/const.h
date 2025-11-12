/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_CONST_H
#define _LINUX_CONST_H

#include <vdso/const.h>

#define __AC(X, Y)	(X##Y)
#define _AC(X, Y)	__AC(X, Y)

#define __is_constexpr(val) (sizeof(int) == sizeof(*(8 ? ((void *)((long)(val) * 0l)) : (int *)8)))

#endif
