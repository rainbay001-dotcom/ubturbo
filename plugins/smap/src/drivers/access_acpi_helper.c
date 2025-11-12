// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 acpi helper module
 */

#include "access_acpi_helper.h"

static enum acpi_sub_type acpi_get_subtable_type(char *id)
{
	if (strncmp(id, ACPI_SIG_HMAT, ACPI_SIG_LENGTH) == 0)
		return SUBTABLE_HMAT;
	return SUBTABLE_COMMON;
}

static unsigned long acpi_get_subtable_header_length(struct acpi_subtable_entry *entry)
{
	switch (entry->type) {
	case SUBTABLE_COMMON:
		return sizeof(entry->hdr->common);
	case SUBTABLE_HMAT:
		return sizeof(entry->hdr->hmat);
	}
	return 0;
}

static unsigned long acpi_get_entry_type(struct acpi_subtable_entry *entry)
{
	switch (entry->type) {
	case SUBTABLE_COMMON:
		return entry->hdr->common.type;
	case SUBTABLE_HMAT:
		return entry->hdr->hmat.type;
	}
	return 0;
}

static unsigned long acpi_get_entry_length(struct acpi_subtable_entry *entry)
{
	switch (entry->type) {
	case SUBTABLE_COMMON:
		return entry->hdr->common.length;
	case SUBTABLE_HMAT:
		return entry->hdr->hmat.length;
	}
	return 0;
}

int acpi_parse_entries_array(char *id, unsigned long table_size,
			     struct acpi_table_header *header,
			     struct acpi_subtable_proc *proc, int num,
			     unsigned int max_entries)
{
	struct acpi_subtable_entry entry;
	unsigned long total_len, subtable_len, entry_len;
	int count = 0;
	int errs = 0;
	int i;

	total_len = (unsigned long)(void *)header + header->length;
	entry.hdr =
		(union acpi_subtable_headers *)(void *)((unsigned long)(void *)
								header +
							table_size);
	entry.type = acpi_get_subtable_type(id);
	subtable_len = acpi_get_subtable_header_length(&entry);

	while (((unsigned long)entry.hdr) + subtable_len < total_len) {
		if (max_entries && count >= max_entries)
			break;

		for (i = 0; i < num; i++) {
			if (acpi_get_entry_type(&entry) !=
			    (unsigned long)proc[i].id)
				continue;
			if (!proc[i].handler ||
			    (!errs && proc[i].handler(entry.hdr, total_len))) {
				errs++;
				continue;
			}

			proc[i].count++;
			break;
		}
		if (i != num)
			count++;

		entry_len = acpi_get_entry_length(&entry);
		if (entry_len == 0)
			return -EINVAL;

		entry.hdr = (union acpi_subtable_headers *)((unsigned long)
								    entry.hdr +
							    entry_len);
	}

	if (max_entries && count > max_entries) {
		pr_warn("[%4.4s:%#x] reach maximum %i entries\n", id, proc->id,
			count);
	}

	return errs ? -EINVAL : count;
}
