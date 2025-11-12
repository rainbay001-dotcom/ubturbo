/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/usb.h>

void usb_put_dev(struct usb_device *dev)
{
}

struct usb_interface *usb_find_interface(struct usb_driver *drv, int minor)
{
    return NULL;
}

void usb_free_coherent(struct usb_device *dev, size_t size, void *addr,
                       dma_addr_t dma)
{
}

void *usb_alloc_coherent(struct usb_device *dev, size_t size, gfp_t mem_flags,
                         dma_addr_t *dma)
{
    return NULL;
}

struct usb_device *usb_get_dev(struct usb_device *dev)
{
    return NULL;
}

int usb_find_common_endpoints(struct usb_host_interface *alt,
                              struct usb_endpoint_descriptor **bulk_in,
                              struct usb_endpoint_descriptor **bulk_out,
                              struct usb_endpoint_descriptor **int_in,
                              struct usb_endpoint_descriptor **int_out)
{
    return 0;
}

void *usb_get_intfdata(struct usb_interface *intf)
{
    return dev_get_drvdata(&intf->dev);
}