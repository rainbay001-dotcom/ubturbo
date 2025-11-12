/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Dynamic loading of modules into the kernel.
 *
 * Rewritten by Richard Henderson <rth@tamu.edu> Dec 1996
 * Rewritten again by Rusty Russell, 2002
 */

#ifndef _LINUX_MODULE_H
#define _LINUX_MODULE_H

#include <linux/export.h>
#include <linux/moduleparam.h>

#define module_init(x)
#define module_exit(x)

#define MODULE_INFO(tag, info)
#define MODULE_ALIAS(_alias)
#define MODULE_SOFTDEP(_softdep)
#define MODULE_LICENSE(_license)
#define MODULE_AUTHOR(_author)
#define MODULE_DESCRIPTION(_description)

#define __init
#define __exit

struct module {
};

#endif /* _LINUX_MODULE_H */

