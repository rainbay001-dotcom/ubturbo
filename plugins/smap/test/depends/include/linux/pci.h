/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_PCI_H
#define LINUX_PCI_H

#include <linux/mod_devicetable.h>
#include <linux/capability.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/pci_ids.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/pci.h>

enum {
	/* See memremap() kernel-doc for usage description... */
	MEMREMAP_WB = 1 << 0,
};

#ifdef __cplusplus
extern "C" {
#endif

struct pci_dev {
	struct device	dev;			/* Generic device interface */
};

#define	to_pci_dev(n) container_of(n, struct pci_dev, dev)

int __must_check pci_enable_device(struct pci_dev *dev);
void pci_disable_device(struct pci_dev *dev);
void pci_set_master(struct pci_dev *dev);

struct pci_dev *pci_get_class(unsigned int class_id, struct pci_dev *from);

struct pci_driver {
	const char *name;
	const struct pci_device_id *id_table;	/* Must be non-NULL for probe to be called */
	int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id);	/* New device inserted */
	void (*remove)(struct pci_dev *dev);	/* Device removed (NULL if not a hot-plug capable driver) */
};

#define pci_register_driver(driver) (driver)
void pci_unregister_driver(struct pci_driver *dev);

static inline void *pci_get_drvdata(struct pci_dev *pdev)
{
}

static inline void pci_set_drvdata(struct pci_dev *pdev, void *data)
{
}

static inline int pci_request_mem_regions(struct pci_dev *pdev, const char *name)
{
	return 0;
}

static inline void pci_release_mem_regions(struct pci_dev *pdev)
{
}

static inline int pci_is_enabled(struct pci_dev *pdev)
{
	return 0;
}

#define pci_resource_start(dev, bar) (dev)
#define pci_resource_len(dev, bar) (dev)

#ifdef __cplusplus
}
#endif

#endif /* LINUX_PCI_H */
