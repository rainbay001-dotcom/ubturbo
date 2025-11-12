/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _PLATFORM_DEVICE_H_
#define _PLATFORM_DEVICE_H_

#include <linux/device/driver.h>
#include <linux/device.h>
#include <linux/ioport.h>

struct platform_device {
    const char *name;
    int id;
    bool id_auto;
    struct device dev;
    u64 platform_dma_mask;
};

#define ACPI_PTR(_ptr)	(_ptr)

struct platform_driver {
    int (*probe)(struct platform_device *);
    int (*remove)(struct platform_device *);
    void (*remove_new)(struct platform_device *);
    void (*shutdown)(struct platform_device *);
    int (*resume)(struct platform_device *);
    struct device_driver driver;
};

#define platform_driver_register(drv) (0)
#define platform_driver_unregister(drv) (0)

static inline struct resource *platform_get_resource(struct platform_device *dev, unsigned int type, unsigned int num)
{
    return NULL;
}

#endif /* _PLATFORM_DEVICE_H_ */
