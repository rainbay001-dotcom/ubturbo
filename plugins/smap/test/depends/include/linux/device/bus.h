/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef TEST_BUS_H
#define TEST_BUS_H

#include <linux/device.h>

#ifdef __cplusplus
extern "C" {
#endif


struct bus_type {
    const char *name;

    int (*match)(struct device *dev, struct device_driver *drv);
    int (*uevent)(struct device *dev);
    int (*probe)(struct device *dev);
    void (*remove)(struct device *dev);
};

int bus_register(struct bus_type *bus);

void bus_unregister(struct bus_type *bus);

#ifdef __cplusplus
}
#endif

#endif // TEST_BUS_H
