/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MISCDEVICE_H
#define _LINUX_MISCDEVICE_H
#include <linux/types.h>

#define MISC_DYNAMIC_MINOR	255

#ifdef __cplusplus
extern "C" {
#endif

struct miscdevice {
    /* Depends this_device */
	struct device *this_device;
	char reserved1;
    /* Depends parent */
	struct device *parent;
	char reserved2;
	/* Depends miscdevice */
	int32_t minor;
	char reserved3;
    /* Depends file_operations */
	const struct file_operations *fops;
	char reserved4;
    /* Depends name */
	const char *name;
	char reserved5;
};

int misc_register(struct miscdevice *misc);
void misc_deregister(struct miscdevice *misc);

#ifdef __cplusplus
}
#endif

#endif
