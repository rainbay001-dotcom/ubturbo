/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/acpi.h>

acpi_status
acpi_get_table(char *signature,
	       u32 instance, struct acpi_table_header ** out_table)
{
	return 0;
}

void acpi_put_table(struct acpi_table_header *table)
{
}
