/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/dmi.h>

u64 dmi_memdev_size(u16 handle)
{
	return 0;
}

u8 dmi_memdev_type(u16 handle)
{
	return 0;
}

u16 dmi_memdev_handle(int slot)
{
	return 0;
}

int dmi_walk(void (*decode)(const struct dmi_header *, void *),
	     void *private_data)
{
	return 0;
}