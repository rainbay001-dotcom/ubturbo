/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_USB_H
#define __LINUX_USB_H

#include <linux/mod_devicetable.h>
#include <uapi/linux/usb/ch9.h>
#include <linux/device.h>
#include <linux/kref.h>
#include <linux/fs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define URB_NO_TRANSFER_DMA_MAP	0x0004

struct usb_device {
    int	devnum;
    struct device dev;
    struct usb_device_descriptor descriptor;
};

struct usb_host_interface {
    char *string;
};

struct usb_interface {
    struct usb_host_interface *cur_altsetting;
    int minor;
    struct device dev;
};

struct usb_driver {
    const char *name;
    int (*probe) (struct usb_interface *intf,
                  const struct usb_device_id *id);
    void (*disconnect) (struct usb_interface *intf);
    const struct usb_device_id *id_table;
    unsigned int supports_autosuspend : 1;
};

struct usb_anchor {
};

struct usb_class_driver {
    char *name;
    const struct file_operations *fops;
    int minor_base;
};

typedef void (*usb_complete_t)(struct urb *);

struct urb {
    struct usb_device *dev;
    unsigned int pipe;
    int status;
    unsigned int transfer_flags;
    void *transfer_buffer;
    dma_addr_t transfer_dma;
    u32 transfer_buffer_length;
    void *context;
    usb_complete_t complete;
};

extern void *usb_get_intfdata(struct usb_interface *intf);

static inline void usb_fill_bulk_urb(struct urb *urb, struct usb_device *dev, unsigned int pipe,
                                     void *transfer_buffer, int buffer_length,
                                     usb_complete_t complete_fn, void *context)
{
    urb->context = context;
    urb->complete = complete_fn;
    urb->transfer_buffer_length = buffer_length;
    urb->transfer_buffer = transfer_buffer;
    urb->pipe = pipe;
    urb->dev = dev;
}

static inline void init_usb_anchor(struct usb_anchor *anchor)
{
}

#define	to_usb_device(d) container_of(d, struct usb_device, dev)
static inline struct usb_device *interface_to_usbdev(struct usb_interface *intf)
{
    return to_usb_device(intf->dev.parent);
}

static inline void usb_set_intfdata(struct usb_interface *intf, void *data)
{
}

void *usb_alloc_coherent(struct usb_device *dev, size_t size,
                         gfp_t mem_flags, dma_addr_t *dma);
int usb_find_common_endpoints(struct usb_host_interface *alt,
                          struct usb_endpoint_descriptor **bulk_in,
                          struct usb_endpoint_descriptor **bulk_out,
                          struct usb_endpoint_descriptor **int_in,
                          struct usb_endpoint_descriptor **int_out);
extern struct usb_device *usb_get_dev(struct usb_device *dev);
extern void usb_put_dev(struct usb_device *dev);
extern struct usb_interface *usb_find_interface(struct usb_driver *drv,
                                                int minor);
void usb_free_coherent(struct usb_device *dev, size_t size,
                       void *addr, dma_addr_t dma);
extern int usb_bulk_msg(struct usb_device *usb_dev, unsigned int pipe,
                        void *data, int len, int *actual_length,
                        int timeout);

extern int usb_autopm_get_interface(struct usb_interface *intf);
extern void usb_autopm_put_interface(struct usb_interface *intf);
extern struct urb *usb_alloc_urb(int iso_packets, gfp_t mem_flags);
extern void usb_anchor_urb(struct urb *urb, struct usb_anchor *anchor);
extern void usb_unanchor_urb(struct urb *urb);
extern int usb_submit_urb(struct urb *urb, gfp_t mem_flags);
extern void usb_free_urb(struct urb *urb);
extern void usb_kill_anchored_urbs(struct usb_anchor *anchor);
extern int usb_register_dev(struct usb_interface *intf,
                            struct usb_class_driver *class_driver);
extern void usb_deregister_dev(struct usb_interface *intf,
                               struct usb_class_driver *class_driver);
static inline unsigned int __create_pipe(struct usb_device *dev,
                                         unsigned int endpoint)
{
    return (dev->devnum << 8) | (endpoint << 15);
}

#define PIPE_BULK			3
#define USB_DIR_IN			0x80		/* to host */

#define usb_sndbulkpipe(dev, endpoint)	\
	((PIPE_BULK << 30) | __create_pipe(dev, endpoint))
#define usb_rcvbulkpipe(dev, endpoint)	\
	((PIPE_BULK << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)

#ifdef __cplusplus
}
#endif

#endif
