/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/kobject.h>

void kobject_put(struct kobject *kobj)
{}

int kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
			 struct kobject *parent, const char *fmt, ...)
{
	return 0;
}
