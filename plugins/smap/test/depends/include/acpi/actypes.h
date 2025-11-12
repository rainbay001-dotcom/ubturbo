/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

#ifndef __ACTYPES_H__
#define __ACTYPES_H__

/*
 * Miscellaneous types
 */
typedef u32 acpi_status;	/* All ACPI Exceptions */
typedef u32 acpi_name;		/* 4-byte ACPI name */
typedef char *acpi_string;	/* Null terminated ASCII string */
typedef void *acpi_handle;	/* Actually a ptr to a NS Node */

#endif				/* __ACTYPES_H__ */
