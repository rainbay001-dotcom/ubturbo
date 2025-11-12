/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Based on arch/arm/include/asm/io.h
 *
 * Copyright (C) 1996-2000 Russell King
 * Copyright (C) 2012 ARM Ltd.
 */
#ifndef __ASM_IO_H
#define __ASM_IO_H

#include <asm/byteorder.h>

#define readl(c) (c)
#define writel(v, c) (c)
#define iounmap(addr) (addr)
#define ioremap(addr, size) (addr)

#endif	/* __ASM_IO_H */
