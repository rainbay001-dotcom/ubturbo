/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/usb.h>

struct urb *usb_alloc_urb(int iso_packets, gfp_t mem_flags)
{
    return NULL;
}

void usb_anchor_urb(struct urb *urb, struct usb_anchor *anchor)
{
}

void usb_unanchor_urb(struct urb *urb)
{
}

int usb_submit_urb(struct urb *urb, gfp_t mem_flags)
{
    return 0;
}

void usb_free_urb(struct urb *urb)
{
}

void usb_kill_anchored_urbs(struct usb_anchor *anchor)
{
}