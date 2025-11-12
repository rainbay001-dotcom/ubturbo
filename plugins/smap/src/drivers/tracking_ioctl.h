/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: tracking_dev
 */

#ifndef __TRACKING_IOCTL_H__
#define __TRACKING_IOCTL_H__

#include <linux/types.h>

#include "drv_common.h"
/*
 *  cmd 0->Tracking able config:
 *      arg 0->Tracking disable
 *      arg 1->Tracking enable
 */

#define SMAP_IOCTL_TRACKING_CMD _IOW('N', 0, unsigned long)

/*
 *  cmd 1->Tracking mode sel:
 *      arg 0->mat actc
 *      arg 1->mebs
 *      arg 2->cpi
 */

#define SMAP_IOCTL_MODE_SET_CMD _IOW('N', 1, unsigned long)

#define SMAP_IOCTL_MAP_MODE_CMD _IOW('N', 2, unsigned long)

/*
 *  cmd 2->Tracking dev get size:
 *
 */

#define SMAP_IOCTL_GET_SIZE_CMD _IOR('N', 3, unsigned long)

#define SMAP_IOCTL_PAGE_SIZE_SET_CMD _IOW('N', 4, unsigned long)

#define SMAP_IOCTL_REINIT_NODE_CMD _IOW('N', 5, unsigned long)

#define SMAP_IOCTL_RAM_CHANGE _IOW('N', 6, int)

#define SMAP_IOCTL_ACPI_LEN _IOW('N', 7, int)

#define SMAP_IOCTL_IOMEM_LEN _IOW('N', 8, int)

#define SMAP_IOCTL_ACPI_ADDR _IOW('N', 9, struct acpi_seg)

#define SMAP_IOCTL_IOMEM_ADDR _IOW('N', 10, struct iomem_seg)

enum node_tracking_cmd {
	TRACKING_DISABLED,
	TRACKING_ENABLED,
};

#endif