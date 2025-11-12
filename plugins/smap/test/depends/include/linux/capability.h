// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 Tiering Memory Solution: tracking_dev
*/

#ifndef TEST_CAPABILITY_H
#define TEST_CAPABILITY_H

#include <uapi/linux/capability.h>
#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool capable(int cap);

#ifdef __cplusplus
}
#endif

#endif // TEST_CAPABILITY_H
