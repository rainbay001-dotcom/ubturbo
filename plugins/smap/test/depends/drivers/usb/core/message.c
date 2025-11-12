/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/usb.h>

int usb_bulk_msg(struct usb_device *usb_dev, unsigned int pipe,
                 void *data, int len, int *actual_length, int timeout)
{
    return 0;
}