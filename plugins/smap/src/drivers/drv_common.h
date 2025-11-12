/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP: tracking_common
 */

#ifndef __TRACKING_COMMON_H__
#define __TRACKING_COMMON_H__

typedef u16 actc_t;

struct acpi_seg {
	int node;
	int pxm;
	u64 start;
	u64 end;
};

struct acpi_msg {
	size_t cnt;
	struct acpi_seg *acpi_seg;
};

struct numa_seg {
	int node;
	u64 start;
	u64 end;
};

struct numa_msg {
	int cnt;
	struct numa_seg *numa_seg;
};

struct iomem_seg {
	int node;
	u64 start;
	u64 end;
};

struct iomem_msg {
	int cnt;
	struct iomem_seg *iomem_seg;
};

#endif /* __TRACKING_COMMON_H__ */