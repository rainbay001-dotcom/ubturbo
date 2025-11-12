/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/usb.h>

int usb_autopm_get_interface(struct usb_interface *intf)
{
    return 0;
}

void usb_autopm_put_interface(struct usb_interface *intf)
{
}