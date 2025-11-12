/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_LOCAL_H
#define _ASM_GENERIC_LOCAL_H

#include <asm-generic/atomic-long.h>

typedef struct {
	atomic_long_t a;
} local_t;

#endif