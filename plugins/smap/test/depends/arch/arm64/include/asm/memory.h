/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_DEPENDS_MEMORY_H
#define __ASM_DEPENDS_MEMORY_H

#include <linux/const.h>
#include <asm/page-def.h>

#define VA_BITS			(CONFIG_ARM64_VA_BITS)
#define _PAGE_OFFSET(va)	(-(UL(1) << (va)))
#define PAGE_OFFSET		(_PAGE_OFFSET(VA_BITS))

#endif /* __ASM_DEPENDS_MEMORY_H */
