/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LINUX_STRINGIFY_DEPENDS_H
#define __LINUX_STRINGIFY_DEPENDS_H

#define __stringify_1(val...)	#val
#define __stringify(val...)	__stringify_1(val)

#endif
