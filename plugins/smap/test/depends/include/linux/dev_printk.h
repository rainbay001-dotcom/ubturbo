// SPDX-License-Identifier: GPL-2.0
#ifndef _DEVICE_PRINTK_H_
#define _DEVICE_PRINTK_H_

#include <linux/device.h>

#ifndef dev_fmt
#define dev_fmt(fmt) fmt
#endif

static inline void dev_printk(const char *level, const struct device *dev,
                const char *fmt, ...) {}

static inline void _dev_err(const struct device *dev, const char *fmt, ...) {}
static inline void _dev_info(const struct device *dev, const char *fmt, ...) {}

#define dev_err(dev, fmt, ...)						\
	_dev_err(dev, dev_fmt(fmt), ##__VA_ARGS__)
#define dev_info(dev, fmt, ...)						\
	_dev_info(dev, dev_fmt(fmt), ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...)						\
	dev_printk(dev, dev_fmt(fmt), ##__VA_ARGS__)

#endif /* _DEVICE_PRINTK_H_ */
