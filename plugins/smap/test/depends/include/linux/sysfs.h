/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sysfs.h - definitions for the device driver filesystem
 *
 * Copyright (c) 2001,2002 Patrick Mochel
 * Copyright (c) 2004 Silicon Graphics, Inc.
 * Copyright (c) 2007 SUSE Linux Products GmbH
 * Copyright (c) 2007 Tejun Heo <teheo@suse.de>
 *
 * Please see Documentation/filesystems/sysfs.rst for more information.
 */

#ifndef _SYSFS_H_
#define _SYSFS_H_

#include <linux/types.h>
#include <linux/stringify.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject_ns.h>
#include <linux/lockdep.h>

#ifdef __cplusplus
extern "C" {
#endif

struct kobject;

struct attribute {
	const char *name;
	umode_t mode;
};

struct attribute_group {
	const char *name;
	struct attribute **attrs;
};

struct sysfs_ops {
	ssize_t	(*show)(struct kobject *, struct attribute *, char *);
	ssize_t	(*store)(struct kobject *, struct attribute *, const char *, size_t);
};


#define __ATTR(_name, _mode, _show, _store) {				\
	.attr = {.name = __stringify(_name),				\
		 .mode = VERIFY_OCTAL_PERMISSIONS(_mode) },		\
	.show	= (_show),						\
	.store	= (_store),						\
}

#define __ATTR_RW(_name) __ATTR(_name, 0644, _name##_show, _name##_store)
#define __ATTR_RO(_name) {						\
	.attr	= { .name = __stringify(_name), .mode = 0444 },		\
	.show	= _name##_show,						\
}

extern int sysfs_create_group(struct kobject *kobj,
			      const struct attribute_group *grp);

extern void sysfs_remove_group(struct kobject *kobj,
			       const struct attribute_group *grp);

void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr);

int sysfs_create_file(struct kobject *kobj, const struct attribute *attr);

static inline int sysfs_create_link(struct kobject *kobj,
				    struct kobject *target, const char *name)
{
	return 0;
}

static inline void sysfs_remove_link(struct kobject *kobj, const char *name)
{
}

#ifdef __cplusplus
}
#endif

#endif /* _SYSFS_H_ */
