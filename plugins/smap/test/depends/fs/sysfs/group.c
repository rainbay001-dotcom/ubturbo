/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/kobject.h>

int sysfs_create_group(struct kobject *kobj,
		       const struct attribute_group *grp)
{
	return 0;
}

void sysfs_remove_group(struct kobject *kobj,
			const struct attribute_group *grp)
{}

void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr)
{}

int sysfs_create_file(struct kobject *kobj, const struct attribute *attr)
{
	return 0;
}
