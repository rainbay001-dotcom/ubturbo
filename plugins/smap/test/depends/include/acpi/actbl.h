/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

#ifndef __ACTBL_H__
#define __ACTBL_H__

struct acpi_table_header {
	u32 length;		/* Length of table in bytes, including this header */
};

/*
 * Get the remaining ACPI tables
 */
#include <acpi/actbl1.h>
#include <acpi/actbl3.h>

#endif				/* __ACTBL_H__ */
