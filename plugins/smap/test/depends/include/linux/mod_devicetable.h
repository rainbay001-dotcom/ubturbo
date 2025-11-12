/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Device tables which are exported to userspace via
 * scripts/mod/file2alias.c.  You must keep that file in sync with this
 * header.
 */

#ifndef LINUX_MOD_DEVICETABLE_H
#define LINUX_MOD_DEVICETABLE_H

#define PCI_ANY_ID (~0)

struct pci_device_id {
};

struct usb_device_id {
};

#endif /* LINUX_MOD_DEVICETABLE_H */

