// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 Tiering Memory Solution: tracking_dev
*/

#ifndef TEST_PAGE_DEF_H
#define TEST_PAGE_DEF_H

#include <linux/const.h>

#define  CONFIG_ARM64_PAGE_SHIFT 12
#define PAGE_SHIFT		CONFIG_ARM64_PAGE_SHIFT
#define PAGE_SIZE		(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

#endif // TEST_PAGE_DEF_H
