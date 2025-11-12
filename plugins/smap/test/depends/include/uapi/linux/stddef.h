/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_STDDEF_H
#define _UAPI_LINUX_STDDEF_H

#include <linux/compiler_types.h>

#define __DECLARE_FLEX_ARRAY(type, name)	\
	struct { \
		struct { } __empty_ ## name; \
		type name[]; \
	}

#define __struct_group(tag, name, attrs, members...) \
	union { \
		struct { members } attrs; \
		struct tag { members } attrs name; \
	}

#ifndef __always_inline
#define __always_inline inline
#endif

#endif
