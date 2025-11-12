/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _LINUX_ACPI_H
#define _LINUX_ACPI_H

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/printk.h>

#ifndef _LINUX
#define _LINUX
#endif

#include <acpi/acpi.h>
#include <asm/acpi.h>

#define ACPI_FAILURE(a)                 (a)
#define ACPI_COMPANION(a)               (a)

typedef int (*acpi_tbl_entry_handler)(union acpi_subtable_headers *header,
				      const unsigned long end);

union acpi_subtable_headers {
	struct acpi_subtable_header common;
	struct acpi_hmat_structure hmat;
};

struct acpi_subtable_proc {
	int id;
	acpi_tbl_entry_handler handler;
	int count;
};

#endif	/* _LINUX_ACPI_H */
