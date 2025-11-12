// SPDX-License-Identifier: GPL-2.0

#ifndef _KOBJECT_DEPENDS_H_
#define _KOBJECT_DEPENDS_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/sysfs.h>

struct kobject {
	unsigned int uevent_suppress;
	unsigned int state_remove_uevent_sent;
	struct list_head entry;
	unsigned int state_initialized;
	/* Depends kobj_attribute */
	unsigned int state_in_sysfs;
	struct kobject *parent;
	const struct kobj_type *ktype;
	const char *name;
	unsigned int state_add_uevent_sent;
};

struct kobj_type {
	void (*release)(struct kobject *kobj);
	const struct sysfs_ops *sysfs_ops;
};

extern struct kobject *mm_kobj;

struct kobj_attribute {
	struct attribute attr;  /* kobj_attribute */
	ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
};

#ifdef __cplusplus
extern "C" {
#endif

extern void kobject_put(struct kobject *kobj);

extern int kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
	struct kobject *parent, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _KOBJECT_H_ */
