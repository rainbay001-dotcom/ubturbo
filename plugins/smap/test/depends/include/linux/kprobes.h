/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _LINUX_KPROBES_H
#define _LINUX_KPROBES_H

#include <asm/kprobes.h>

struct kprobe;

struct kprobe {
	kprobe_opcode_t *addr;
	const char *symbol_name;
};

#endif /* _LINUX_KPROBES_H */
