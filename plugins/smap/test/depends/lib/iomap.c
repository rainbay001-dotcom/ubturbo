// SPDX-License-Identifier: GPL-2.0

#include <linux/compiler_types.h>
#include <linux/types.h>

unsigned int ioread16(const void __iomem *addr)
{
	return 0;
}

void iowrite16(u16 val, void __iomem *addr)
{
}

unsigned int ioread32(const void __iomem *addr)
{
	return 0xffffffff;
}

void iowrite32(u32 val, void __iomem *addr)
{
}
