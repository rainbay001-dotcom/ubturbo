/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_SPARSEMEM_DEPENDS_H
#define __ASM_SPARSEMEM_DEPENDS_H

#ifdef CONFIG_ARM64_64K_PAGES
#define SECTION_SIZE_BITS 29
#else
#define SECTION_SIZE_BITS 27
#endif

#endif