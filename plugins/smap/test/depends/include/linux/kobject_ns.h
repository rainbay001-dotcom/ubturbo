// SPDX-License-Identifier: GPL-2.0

#ifndef _LINUX_KOBJECT_NS_DEPENDS_H
#define _LINUX_KOBJECT_NS_DEPENDS_H

struct kobject;
struct sock;

enum kobj_ns_type {
	KOBJ_NS_TYPE_NONE = 0,
	KOBJ_NS_TYPE_NET,
	KOBJ_NS_TYPES
};

struct kobj_ns_type_operations {
	/* depends current_may_mount */
	bool (*current_may_mount)(void);
	char reserver1;
	/* depends grab_current_ns */
	void *(*grab_current_ns)(void);
	char reserver2;
	/* depends netlink_ns */
	const void *(*netlink_ns)(struct sock *sock_name);
	char reserver3;
	/* kobj_ns_type_operations */
	const void *(*initial_ns)(void);
	char reserver4;
	/* depends drop_ns */
	void (*drop_ns)(void *);
	char reserver5;
	enum kobj_ns_type type;
	char reserver6;
};

void kobj_ns_drop(enum kobj_ns_type type, void *ns);
const void *kobj_ns_initial(enum kobj_ns_type type);
const void *kobj_ns_netlink(enum kobj_ns_type type, struct sock *sk);
void *kobj_ns_grab_current(enum kobj_ns_type type);
bool kobj_ns_current_may_mount(enum kobj_ns_type type);

const struct kobj_ns_type_operations *kobj_ns_ops(struct kobject *kobj);
const struct kobj_ns_type_operations *kobj_child_ns_ops(struct kobject *parent);
int kobj_ns_type_registered(enum kobj_ns_type type);
int kobj_ns_type_register(const struct kobj_ns_type_operations *ops);

#endif
