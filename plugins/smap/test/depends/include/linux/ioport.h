/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _LINUX_IOPORT_DEPENDS_H
#define _LINUX_IOPORT_DEPENDS_H

#include <linux/types.h>

#define IORESOURCE_MEM		0x00000200
#define IORESOURCE_SYSRAM       0x01000000
#define IORESOURCE_SYSTEM_RAM           (IORESOURCE_MEM|IORESOURCE_SYSRAM)

struct resource {
    /* description */
    unsigned long desc;
	unsigned long flags;
    const char *name;
    /* start and end */
    resource_size_t end;
	resource_size_t start;
    /* resource tree pointer */
	struct resource *parent;
    struct resource *sibling;
    struct resource *child;
};

/* IORESOURCE_SYSRAM specific bits. */
#define IORESOURCE_SYSRAM_DRIVER_MANAGED        0x02000000

enum {
        IORES_DESC_NONE
};

extern struct resource iomem_resource;

#endif
