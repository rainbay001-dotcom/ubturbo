/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PFN_DEPENDS_H_
#define _LINUX_PFN_DEPENDS_H_

#ifndef __ASSEMBLY__
#include <linux/types.h>

typedef struct {
	u64 val;
} pfn_t;
#endif

#include <asm/memory.h>
#define PHYS_PFN(x)	((unsigned long)((x) >> PAGE_SHIFT))
#define PFN_PHYS(x)	((phys_addr_t)(x) << PAGE_SHIFT)
#define PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_ALIGN(x)	(((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)

#endif
