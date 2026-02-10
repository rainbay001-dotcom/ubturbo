/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_KERNEL_DEPENDS_H
#define _LINUX_KERNEL_DEPENDS_H

#include <uapi/linux/kernel.h>
#include <linux/printk.h>
#include <linux/stdarg.h>
#include <linux/bitops.h>
#include <linux/minmax.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/rtmutex.h>
#include <linux/rwbase_rt.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock_types_raw.h>
#include <linux/io.h>
#include <linux/pfn.h>
#include <linux/mmzone.h>
#include <linux/memory_hotplug.h>
#include <linux/rwlock_rt.h>
#include <linux/dma-direction.h>
#include <linux/workqueue.h>
#include <linux/rwlock.h>

#define ALIGN(x, a)		__ALIGN_KERNEL((x), (a))

#define DIV_ROUND_UP __KERNEL_DIV_ROUND_UP

#endif
