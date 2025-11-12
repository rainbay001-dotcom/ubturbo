/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/miscdevice.h>

int misc_register(struct miscdevice *misc)
{
	return 0;
}

void misc_deregister(struct miscdevice *misc)
{
	return;
}
