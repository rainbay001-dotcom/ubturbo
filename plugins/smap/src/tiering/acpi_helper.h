/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 acpi helper header
 */

#ifndef _ACPI_HELPER_H
#define _ACPI_HELPER_H

#include <linux/acpi.h>

#define ACPI_SIG_LENGTH 4

enum acpi_sub_type {
	SUBTABLE_COMMON,
	SUBTABLE_HMAT,
};

struct acpi_subtable_entry {
	union acpi_subtable_headers *hdr;
	enum acpi_sub_type type;
};

int acpi_parse_entries_array(char *id, unsigned long table_size,
			     struct acpi_table_header *table_header,
			     struct acpi_subtable_proc *proc, int proc_num,
			     unsigned int max_entries);

#endif /* _ACPI_HELPER_H */
