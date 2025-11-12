/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI__LINUX_USB_CH9_H
#define _UAPI__LINUX_USB_CH9_H

#include <linux/types.h>	/* __u8 etc */

struct usb_device_descriptor {
    __le16 bcdDevice;
} __attribute__ ((packed));

struct usb_endpoint_descriptor {
    __u8  bEndpointAddress;
} __attribute__ ((packed));

static inline int usb_endpoint_maxp(const struct usb_endpoint_descriptor *epd)
{
    return 0;
}

#endif
