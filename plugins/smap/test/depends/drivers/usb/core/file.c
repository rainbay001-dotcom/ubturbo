/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/usb.h>

int usb_register_dev(struct usb_interface *intf,
                     struct usb_class_driver *class_driver)
{
    return 0;
}

void usb_deregister_dev(struct usb_interface *intf,
                        struct usb_class_driver *class_driver)
{
}