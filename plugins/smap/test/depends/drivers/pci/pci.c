/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * PCI Bus Services, see include/linux/pci.h for further explanation.
 *
 * Copyright 1993 -- 1997 Drew Eckhardt, Frederic Potter,
 * David Mosberger-Tang
 *
 * Copyright 1997 -- 2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/pci.h>

int pci_enable_device(struct pci_dev *dev)
{
	return 0;
}

void pci_disable_device(struct pci_dev *dev)
{
}

void pci_set_master(struct pci_dev *dev)
{
}