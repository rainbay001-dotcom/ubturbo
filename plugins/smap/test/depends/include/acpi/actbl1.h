/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

#ifndef __ACTBL1_H__
#define __ACTBL1_H__

#define ACPI_SIG_PCCS           "PCC"
#define ACPI_SIG_S3PT           "S3PT"
#define ACPI_SIG_IBFT           "IBFT"
#define ACPI_SIG_HPET           "HPET"
#define ACPI_SIG_HMAT           "HMAT"
#define ACPI_SIG_HEST           "HEST"
#define ACPI_SIG_GTDT           "GTDT"
#define ACPI_SIG_FPDT           "FPDT"
#define ACPI_SIG_ERST           "ERST"
#define ACPI_SIG_EINJ           "EINJ"
#define ACPI_SIG_ECDT           "ECDT"
#define ACPI_SIG_DRTM           "DRTM"
#define ACPI_SIG_DMAR           "DMAR"
#define ACPI_SIG_DBGP           "DBGP"
#define ACPI_SIG_DBG2           "DBG2"
#define ACPI_SIG_CSRT           "CSRT"
#define ACPI_SIG_CPEP           "CPEP"
#define ACPI_SIG_BOOT           "BOOT"
#define ACPI_SIG_BGRT           "BGRT"
#define ACPI_SIG_BERT           "BERT"
#define ACPI_SIG_ASF            "ASF!"

struct acpi_subtable_header {
	u8 type;
	u8 length;
};

struct acpi_hmat_structure {
	u16 type;
	u16 reserved;
	u32 length;
};

#endif
