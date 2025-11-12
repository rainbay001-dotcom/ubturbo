/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/ioport.h>

// Define a resource structure for I/O memory
struct resource iomem_resource = {
    // Name of the resource
    .name	= "PCI mem",

    // Start address of the resource
    .start	= 0,

    // End address of the resource
    .end	= -1,

    // Flags indicating the type of resource
    .flags	= IORESOURCE_MEM,
};
int walk_iomem_res_desc(unsigned long desc, unsigned long flags, u64 start,
                u64 end, void *arg, int (*func)(struct resource *, void *))
{
	return 0;
}
