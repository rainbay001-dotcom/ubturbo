/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

#ifndef __ACTBL3_H__
#define __ACTBL3_H__

#define ACPI_SIG_XXXX           "XXXX"
#define ACPI_SIG_XENV           "XENV"
#define ACPI_SIG_WSMT           "WSMT"
#define ACPI_SIG_WPBT           "WPBT"
#define ACPI_SIG_WDRT           "WDRT"
#define ACPI_SIG_WDDT           "WDDT"
#define ACPI_SIG_WDAT           "WDAT"
#define ACPI_SIG_WAET           "WAET"
#define ACPI_SIG_VRTC           "VRTC"
#define ACPI_SIG_UEFI           "UEFI"
#define ACPI_SIG_TPM2           "TPM2"
#define ACPI_SIG_TCPA           "TCPA"
#define ACPI_SIG_STAO           "STAO"
#define ACPI_SIG_SRAT           "SRAT"
#define ACPI_SIG_SPMI           "SPMI"
#define ACPI_SIG_SPCR           "SPCR"
#define ACPI_SIG_SLIT           "SLIT"
#define ACPI_SIG_SLIC           "SLIC"

enum acpi_srat_type {
    ACPI_SRAT_TYPE_RESERVED = 6,
    ACPI_SRAT_TYPE_GENERIC_AFFINITY = 5,
    ACPI_SRAT_TYPE_GIC_ITS_AFFINITY = 4,
    ACPI_SRAT_TYPE_GICC_AFFINITY = 3,
    ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY = 2,
    ACPI_SRAT_TYPE_MEMORY_AFFINITY = 1,
	ACPI_SRAT_TYPE_CPU_AFFINITY = 0
};

struct acpi_table_srat {
	struct acpi_table_header header;
	u32 table_revision;
	u64 reserved;
};

struct acpi_srat_mem_affinity {
    /* depends acpi_subtable_header */
	struct acpi_subtable_header header;
	char reserved;
	/* depends proximity_domain */
	u32 proximity_domain;
	/* should be 0 */
	char reserved1;
	char reserved2;
	/* acpi_srat_mem_affinity member */
	u64 length;
	char reserved3;
	/* depends base_address */
	u64 base_address;
	char reserved4;
	/* depends flags */
	u32 flags;
	char reserved5;
	char reserved6;
};

#define ACPI_SRAT_MEM_ENABLED       (1)	/* 00: Use affinity structure */
#define ACPI_SRAT_MEM_HOT_PLUGGABLE (1<<1)	/* 01: Memory region is hot pluggable */

#endif
