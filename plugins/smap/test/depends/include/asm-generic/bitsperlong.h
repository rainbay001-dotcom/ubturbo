/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_DEPENDS_GENERIC_BITS_PER_LONG
#define __ASM_DEPENDS_GENERIC_BITS_PER_LONG

#include <uapi/asm-generic/bitsperlong.h>

#ifndef BITS_PER_LONG_LONG
#define BITS_PER_LONG_LONG 64
#endif

#ifdef CONFIG_64BIT
#define BITS_PER_LONG 64
#else
#define BITS_PER_LONG 32
#endif

#endif
