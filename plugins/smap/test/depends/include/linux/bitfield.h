// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: bitfield.h
*/
#ifndef _LINUX_BITFIELD_H
#define _LINUX_BITFIELD_H

#include <asm/byteorder.h>
#include <linux/build_bug.h>

#define FIELD_MAX(_mask) ((_mask) != 0)

#define FIELD_FIT(_mask, _val) ((_mask) != 0)

#define FIELD_GET(_mask, _reg) ((_mask) != 0)

#endif
