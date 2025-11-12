// SPDX-License-Identifier: GPL-2.0

#ifndef _DEVICE_CLASS_DEPENDS_H_
#define _DEVICE_CLASS_DEPENDS_H_

#include <linux/kobject.h>
#include <linux/version.h>

struct class_stub {
};

#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
#define class_create(name) ((name) ? 1 : 0)
#else
#define class_create(owner, name) (((owner) && (name)) ? 1 : 0)
#endif

#ifdef __cplusplus
extern "C" {
#endif
void class_destroy(struct class_stub *cls);
#ifdef __cplusplus
}
#endif

#endif
