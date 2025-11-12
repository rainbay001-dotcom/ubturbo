/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_DEPENDS_CURRENT_H
#define __ASM_DEPENDS_CURRENT_H

#include <linux/compiler.h>

#ifndef __ASSEMBLY__

struct task_struct;

static struct task_struct *get_current(void)
{
	unsigned long depends_sp_el0;

	asm ("mrs %0, sp_el0" : "=r" (depends_sp_el0));

	return (struct task_struct *)depends_sp_el0;
}

#define current get_current()

#endif

#endif

